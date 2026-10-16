struct PS_INPUT
{
    float4 Color : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}