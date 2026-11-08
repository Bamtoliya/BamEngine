struct VS_INPUT
{
    float3 Position : POSITION;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 NDC : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.NDC = input.Position.xy;
    output.Position = float4(input.Position.xy, 1.0f, 1.0f);
    return output;
}