//-----------------------------------------------------------------------------
// author: Dmytro Shchukin
//-----------------------------------------------------------------------------
#include "depth_texture.h"
#include <nvapi.h>

#include "settings.h"

#define FOURCC_RESZ ((D3DFORMAT)(MAKEFOURCC('R','E','S','Z')))
#define FOURCC_INTZ ((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')))
#define FOURCC_RAWZ ((D3DFORMAT)(MAKEFOURCC('R','A','W','Z')))
#define RESZ_CODE 0x7fa05000

//--------------------------------------------------------------------------------------
DepthTexture::DepthTexture(const LPDIRECT3D9 d3d) {
	D3DDISPLAYMODE currentDisplayMode;
	d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &currentDisplayMode);

	// determine if RESZ is supported
	m_isRESZ = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		currentDisplayMode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, FOURCC_RESZ) == D3D_OK;
	SDLOG(-1, "[DepthTexture] RESZ: %s\n", m_isRESZ?"true":"false");

	// determine if INTZ is supported
	m_isINTZ = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		currentDisplayMode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, FOURCC_INTZ) == D3D_OK;
	SDLOG(-1, "[DepthTexture] INTZ: %s\n", m_isINTZ ? "true" : "false");

	// determine if RAWZ is supported, used in GeForce 6-7 series.
	m_isRAWZ = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		currentDisplayMode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, FOURCC_RAWZ) == D3D_OK;
	SDLOG(-1, "[DepthTexture] RAWZ: %s\n", m_isRAWZ ? "true" : "false");

	// determine if RESZ or NVAPI supported
	m_isSupported = (NvAPI_Initialize() == NVAPI_OK || m_isRESZ) && (m_isRAWZ || m_isINTZ);
}

//--------------------------------------------------------------------------------------
void DepthTexture::createTexture(const LPDIRECT3DDEVICE9 device, int width, int height) {
	if(m_isSupported) {
		D3DFORMAT format = m_isINTZ ? FOURCC_INTZ : FOURCC_RAWZ;

		device->CreateTexture(width, height, 1,	D3DUSAGE_DEPTHSTENCIL, format, D3DPOOL_DEFAULT, &m_pTexture, NULL);

		if(m_pTexture) m_pTexture->GetSurfaceLevel(0, &m_pSurface);

		if(!m_isRESZ) {
			NvAPI_D3D9_RegisterResource(m_pTexture);
			NvAPI_D3D9_RegisterResource(m_pSurface);
		}
	}
}

//--------------------------------------------------------------------------------------
DepthTexture::~DepthTexture() {
	if(!m_isRESZ) {
		if(m_pTexture != NULL) NvAPI_D3D9_UnregisterResource(m_pTexture);
		if(m_pSurface != NULL) NvAPI_D3D9_UnregisterResource(m_pSurface);
	}
	SAFERELEASE(m_pSurface);
	SAFERELEASE(m_pTexture);
}

//--------------------------------------------------------------------------------------
void DepthTexture::resolveDepth(const LPDIRECT3DDEVICE9 device) {
	if(m_isRESZ) {
		device->SetVertexShader(NULL);
		device->SetPixelShader(NULL);
		device->SetFVF(D3DFVF_XYZ);
		// Bind depth stencil texture to texture sampler 0
		device->SetTexture(0, m_pTexture);
		// Perform a dummy draw call to ensure texture sampler 0 is set before the // resolve is triggered
		// Vertex declaration and shaders may need to me adjusted to ensure no debug
		// error message is produced
		D3DXVECTOR3 vDummyPoint(0.0f, 0.0f, 0.0f);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
		device->DrawPrimitiveUP(D3DPT_POINTLIST, 1, vDummyPoint, sizeof(D3DXVECTOR3));
		device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		device->SetRenderState(D3DRS_ZENABLE, TRUE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);

		// Trigger the depth buffer resolve; after this call texture sampler 0
		// will contain the contents of the resolve operation
		device->SetRenderState(D3DRS_POINTSIZE, RESZ_CODE);

		// This hack to fix resz hack, has been found by Maksym Bezus!!!
		// Without this line resz will be resolved only for first frame
		device->SetRenderState(D3DRS_POINTSIZE, 0); // TROLOLO!!!
	}
	else {
		// This doesn't work, obviously. Use temporary surface? If so, what format?
		NvAPI_D3D9_StretchRectEx(device, m_pSurface, NULL, m_pTexture, NULL, D3DTEXF_LINEAR);
	}
}
