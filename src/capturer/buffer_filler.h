/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-26 10:35:06
 * @Description:
 */
/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-26 10:35:06
 * @Description:
 */
#ifndef __BUFFER_FILLER_H__
#define __BUFFER_FILLER_H__
#include <d3d11.h>
#include <vector>

class BufferFiller {
public:
    bool Fill(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc, int maxCnt = 5);
    bool Reset();
    ID3D11Texture2D* GetCopy() { return _buffers[_copyIdx]; }
    ID3D11Texture2D* GetMap() { return _buffers[_mapIdx]; }
    void Clear();
    ~BufferFiller()
    {
        Clear();
    }

private:
    int _mapIdx = 0;
    int _copyIdx = 0;
    std::vector<ID3D11Texture2D*> _buffers;
    ID3D11Device* _device = nullptr;
    D3D11_TEXTURE2D_DESC _desc;
};

#endif;