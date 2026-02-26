# Render the wonderful-sphere scene through every available backend.
# Add or remove backends to match what is installed on this machine.

RENDER_RESOLUTION = (512, 512)  # override the harness default for this test

render("scene.usda", "out-arnold.exr", renderer="Arnold")

outputs = ["out-arnold.exr"]