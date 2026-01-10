#pragma once

class RHI;
class Renderer 
{
public:
	Renderer();
	~Renderer();
	void Initialize();
	void Draw();
	void RenderFrame();
	void Shutdown();

private:
	RHI* m_RHI;

};