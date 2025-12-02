// REFERENCE SCRIPT, UNUSED 

// cbuffer: a constant buffer holds constants that remain the same across multiple shader invocations
// the GPU can  access all varuables in a constant buffer at once, making it more efficient than using individual variables

// register(b#): specifies the binding slot for the constant buffer in the shader
// this is how HLSL code specifies where the CPU application should bind the constant buffer
// The register numbers (b0, b1, b2) correspond to different constant buffers that can be set from the application side
// the shader compiler will still do this automatically if no register is specified but doing this provides more control

// matrix: a 4x4 matrix data type commonly used in 3D graphics for transformations such as translation, rotation, scaling, and projection

// PerApplication: holds constants that remain the same for the entire application runtime
cbuffer PerApplication : register(b0)
{
    // Projection matrix for transforming 3D coordinates to 2D screen space
    matrix projectionMatrix;
}

// PerFrame: holds constants that may change every frame but remain the same for all objects in that frame
cbuffer PerFrame : register(b1)
{
    // View matrix for transforming world coordinates to camera/view space
    matrix viewMatrix;
}

// PerObject: holds constants that may change for each individual object being rendered
cbuffer PerObject : register(b2)
{
    // World matrix for transforming object coordinates to world space
    matrix worldMatrix;
}


// input attributes for the vertex shader
// POSITION and COLOR are semantics used to connect the application's vertex data to the shader's input
struct AppData
{
    float3 position : POSITION;
    float3 color : COLOR;
};


// output attributes from the vertex shader to the pixel shader
// at minimum, the output must include SV_POSITION to indicate the vertex position in screen space
struct VertexShaderOutput
{
    float4 color : COLOR;
    // SV_POSITION is a system value semantic that indicates the position of the vertex in screen space
    float4 position : SV_POSITION;
};


// main vertex shader function (entry point)
// takes in AppData and outputs VertexShaderOutput
VertexShaderOutput SimpleVertexShader(AppData IN)
{
    VertexShaderOutput OUT;

    // Combine the world, view, and projection matrices to transform the vertex position
    // The order of multiplication is important
    // mul(viewMatrix, worldMatrix) first transforms the vertex from object (world) space to view (camera) space
    // then mul(projectionMatrix, ...) transforms it from view space to projection (clip) space
    matrix mvp = mul(projectionMatrix, mul(viewMatrix, worldMatrix));
    
    // Transform the vertex position using the combined matrix
    // 3D position is converted to 4D by adding a w component of 1.0
    // The resulting position is in clip space
    // SV_POSITION semantic tells the GPU that this is the final, processed coordinate that will be used by the rasterizer to determine where the pixel will be drawn on screen
    OUT.position = mul(mvp, float4(IN.position, 1.0f));
    
    // Pass through the vertex color to the pixel shader
    OUT.color = float4(IN.color, 1.0f);

    return OUT;
}