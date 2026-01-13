#include "BamEngine/RHI.h"

class DirectXRHI : public RHI
{
public:
	void Init() override;
	void Shutdown() override;
	void SetClearColor(const glm::vec4& color) override;
	void DrawIndexed(uint32 count) override;
	void CreateTexture(void* data, uint32 size) override;
private:

};