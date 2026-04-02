[vk::image_format("rgba8")] RWTexture2D<float4> outputImage;

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 coord: SV_DispatchThreadID) {
    // get image size
    uint width, height;
    outputImage.GetDimensions(width, height);

    if (coord.x >= width || coord.y >= height) return;

    int squareSize = 32;
    int x = coord.x / squareSize;
    int y = coord.y / squareSize;

    int checker = (x + y) % 2;

    float c = checker == 0 ? 1 : 0;

    outputImage[int2(coord.xy)].rgba = float4(c, c, c, 1);
}
