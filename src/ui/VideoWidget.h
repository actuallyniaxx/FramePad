#pragma once

#include "media/MediaTypes.h"

#include <QRhiWidget>

#include <memory>

class QRhi;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;

namespace fp {

// Shows decoded frames on the GPU (Direct3D 11 through Qt RHI). The YUV planes are uploaded
// as three textures and converted to RGB in a shader, so 4K needs no per-pixel CPU work.
class VideoWidget : public QRhiWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override;

    void setFrame(VideoFramePtr frame); // nullptr clears to black

protected:
    void initialize(QRhiCommandBuffer *cb) override;
    void render(QRhiCommandBuffer *cb) override;
    void releaseResources() override;

private:
    void createTextures(int width, int height);

    QRhi *m_rhi = nullptr;
    std::unique_ptr<QRhiTexture> m_planes[3];
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiBuffer> m_params;
    std::unique_ptr<QRhiShaderResourceBindings> m_bindings;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;
    VideoFramePtr m_frame;
    bool m_uploadNeeded = false;
};

} // namespace fp
