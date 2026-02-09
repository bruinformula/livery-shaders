# Bruin Formula Shaders Library

**UNDER CONSTRUCTION**

Contains
- A collection of OSL shaders for cars and CAD. 
- Generation of MaterialX impl bindings of said shaders
- Flattening MaterialX documents into oso files 

The OSL shaders can be compiled by themselves with the `-DSOLO_SHADER` preprocessor macro. EX:
`oslc shaders/CarbonFiber.osl -DSOLO_SHADER`