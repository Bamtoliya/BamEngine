struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT main(uint VertexID : SV_VertexID)
{
    VS_OUTPUT output;
    
    output.TexCoord = float2((VertexID << 1) & 2, VertexID & 2);
    
    output.Position = float4(output.TexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
    
#if defined(_DX12_)
    output.TexCoord.y = 1.0f - output.TexCoord.y;
#endif
    return output;
}