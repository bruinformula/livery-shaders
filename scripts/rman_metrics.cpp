// This is a test to list every metric in RenderMan's Roz stats database

#include <prmanapi.h>
#include <ri.h>
#include <stats/Roz.h>
#include <stats/Session.h>
#include <stats/SessionConfig.h>
#include <stats/Listener.h>
#include <stats/MetricProperties.h>
#include <stats/Payload.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std;

static const char* sessionName = "rman_metrics";

// Keeps the latest value of every metric it is told about. New metrics are queued from
// MetricAddedCallback and subscribed to from another thread (the callback must not
// call back into the session).
class Recorder : public stats::Listener {
public:
    Recorder() : stats::Listener("rman_metrics") { SetEnabled(true); }

    void MetricAddedCallback(stats::Session&, stats::MetricId id) override {
        lock_guard<mutex> lock(m);
        pending.push_back(id);
    }
    void SampleCallback(stats::Session& s, stats::MetricId id, uint64_t, stats::BufferType* p) override {
        store(s, id, p);
    }
    void EventCallback(stats::Session& s, stats::ContextId, stats::MetricId id, uint64_t,
                       stats::BufferType* p) override {
        store(s, id, p);
    }
    void PerContextSampleCallback(stats::Session& s, stats::ContextId, stats::MetricId id, uint64_t,
                                  stats::BufferType* p) override {
        store(s, id, p);
    }

    void subscribe(stats::Session& session, const vector<stats::MetricId>& ids) {
        for (const stats::MetricId& id : ids) {
            stats::Session::ObservationControls controls;
            controls.samplingIntervalInMS = 1000;
            controls.sampleImmediately = true;
            controls.reportingRate = 0;   // the metric's own rate, not every single event
            controls.important = true;
            controls.stochasticOK = false;
            session.DeclareInterest(Id(), id, controls);
        }
    }

    vector<stats::MetricId> takePending() {
        lock_guard<mutex> lock(m);
        vector<stats::MetricId> ids;
        ids.swap(pending);
        return ids;
    }

    string value(const string& name) {
        lock_guard<mutex> lock(m);
        const auto it = values.find(name);
        return it == values.end() ? "-" : it->second;
    }

private:
    void store(stats::Session& session, stats::MetricId id, stats::BufferType* payload) {
        if (!payload) return;
        const stats::MetricProperties props = session.GetMetricProperties(id);
        const string text = stats::Payload::AsString(props.PayloadFormat(), payload);
        lock_guard<mutex> lock(m);
        values[props.Name()] = text;
    }

    mutex m;
    vector<stats::MetricId> pending;
    map<string, string> values;
};

// Metrics only exist while a renderer runs, so this renders a 64x64 quad 
// in-process to the "null" display driver
static void renderQuad() {
    RiBegin(RI_RENDER);
    RiFormat(64, 64, 1.0f);
    RiDisplay("rman_metrics", "null", "rgba", RI_NULL);
    RtInt maxSamples = 4;
    RiHider("raytrace", "int maxsamples", &maxSamples, RI_NULL);
    RiIntegrator("PxrPathTracer", "pt", RI_NULL);
    RtFloat fov = 40.0f;
    RiProjection("perspective", "float fov", &fov, RI_NULL);
    RiTranslate(0.0f, 0.0f, 5.0f);
    RiWorldBegin();
    RiLight("PxrDomeLight", "dome", RI_NULL);
    RiBxdf("PxrDiffuse", "diffuse", RI_NULL);
    RtInt nverts[1] = {4};
    RtInt verts[4] = {0, 1, 2, 3};
    RtFloat P[12] = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
    RiPointsPolygons(1, nverts, verts, "P", P, RI_NULL);
    RiWorldEnd();
}

int main(int argc, char* argv[]) {
    const string variant = argc > 1 ? argv[1] : "xpucpu";
    const string pattern = argc > 2 ? argv[2] : ".*";

    stats::Session& session = stats::AddSession(stats::SessionConfig(sessionName));
    Recorder recorder;
    session.AttachListener(&recorder);

    // argv[0] "prman" sets RenderMan up like the prman command line program.
    string args[] = {"prman", "-statssession", sessionName, "-variant", variant};
    char* argp[5];
    for (int i = 0; i < 5; ++i) argp[i] = args[i].data();
    if (PRManBegin(5, argp) != 0) {
        fprintf(stderr, "PRManBegin failed\n");
        return 1;
    }

    // Subscribe to metrics as they appear, so event metrics fired during the render are seen.
    atomic<bool> rendering{true};
    thread pump([&]() {
        while (rendering) {
            recorder.subscribe(session, recorder.takePending());
            this_thread::sleep_for(chrono::milliseconds(5));
        }
    });
    renderQuad();
    rendering = false;
    pump.join();

    // One last sample of everything, while the renderer still exist.
    vector<stats::MetricId> ids;
    session.QueryRegisteredMetrics(vector<string>{pattern}, ids);
    recorder.subscribe(session, ids);
    this_thread::sleep_for(chrono::milliseconds(500));

    vector<stats::MetricProperties> all;
    for (const stats::MetricId& id : ids) {
        const stats::MetricProperties props = session.GetMetricProperties(id);
        if (props.Name().size()) all.push_back(props);
    }
    sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.Name() < b.Name(); });

    printf("%zu metrics in session '%s' (%s); kind: s sampled, e event, c constant, p per context\n",
           all.size(), sessionName, variant.c_str());
    for (const stats::MetricProperties& props : all) {
        string kind = "----";
        if (props.IsSampled()) kind[0] = 's';
        if (props.IsEventBased()) kind[1] = 'e';
        if (props.IsConstant()) kind[2] = 'c';
        if (props.IsPerContext()) kind[3] = 'p';
        printf("\n%s  %s  %s\n", props.Name().c_str(), kind.c_str(),
               stats::Payload::FormatString(props.PayloadFormat()).c_str());
        printf("    value: %s\n", recorder.value(props.Name()).c_str());
        if (!props.Description().empty()) printf("    %s\n", props.Description().c_str());
    }
    fflush(stdout);

    session.DetachListener(&recorder);
    RiEnd();
    PRManEnd();
    return 0;
}
