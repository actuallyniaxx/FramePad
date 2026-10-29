#include "ui/VideoWidget.h"

#include <QFile>
#include <rhi/qrhi.h>

namespace fp {

namespace {

QShader loadShader(const QString &name)
{
    QFile file(name);
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader();
}

} // namespace

VideoWidget::VideoWidget(QWidget *parent)
    : QRhiWidget(parent)
{
    setApi(QRhiWidget::Api::Direct3D11);
    setMinimumSize(320, 180);
}

VideoWidget::~VideoWidget() = default;

void VideoWidget::setFrame(VideoFramePtr frame)
{
    m_frame = std::move(frame);
    m_uploadNeeded = m_frame != nullptr;
    update();
}

void VideoWidget::initialize(QRhiCommandBuffer *)
{
    // Called again on every resize; the GPU objects only need rebuilding when the QRhi itself changes.
    if (m_rhi != rhi()) {
        releaseResources();
        m_rhi = rhi();
    }
    if (m_pipeline)
        return;

    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_sampler->create();
    m_params.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 4 * sizeof(float)));
    m_params->create();
    createTextures(16, 16);

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/shaders/yuv.vert.qsb"))},
        {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/shaders/yuv.frag.qsb"))},
    });
    m_pipeline->setShaderResourceBindings(m_bindings.get());
    m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_pipeline->create();
    m_uploadNeeded = m_frame != nullptr;
}

void VideoWidget::createTextures(int width, int height)
{
    const QSize luma(width, height);
    const QSize chroma((width + 1) / 2, (height + 1) / 2);
    for (int i = 0; i < 3; ++i) {
        m_planes[i].reset(m_rhi->newTexture(QRhiTexture::R8, i == 0 ? luma : chroma));
        m_planes[i]->create();
    }

    if (!m_bindings)
        m_bindings.reset(m_rhi->newShaderResourceBindings());
    const auto stage = QRhiShaderResourceBinding::FragmentStage;
    m_bindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, stage, m_params.get()),
        QRhiShaderResourceBinding::sampledTexture(1, stage, m_planes[0].get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, stage, m_planes[1].get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(3, stage, m_planes[2].get(), m_sampler.get()),
    });
    m_bindings->create();
}

void VideoWidget::render(QRhiCommandBuffer *cb)
{
    QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();
    if (m_uploadNeeded && m_frame) {
        m_uploadNeeded = false;
        if (m_planes[0]->pixelSize() != QSize(m_frame->width, m_frame->height))
            createTextures(m_frame->width, m_frame->height);
        for (int i = 0; i < 3; ++i) {
            updates->uploadTexture(m_planes[i].get(),
                                   QRhiTextureUploadDescription(QRhiTextureUploadEntry(
                                       0, 0, QRhiTextureSubresourceUploadDescription(m_frame->planes[i]))));
        }
        const float params[4] = {m_frame->bt709 ? 1.0f : 0.0f, m_frame->fullRange ? 1.0f : 0.0f, 0.0f, 0.0f};
        updates->updateDynamicBuffer(m_params.get(), 0, sizeof params, params);
    }

    const QSize output = renderTarget()->pixelSize();
    cb->beginPass(renderTarget(), Qt::black, {1.0f, 0}, updates);
    if (m_frame) {
        // Letterbox: fit the picture (with its pixel aspect ratio) inside the widget.
        const QSizeF picture(m_frame->width * m_frame->pixelAspect, m_frame->height);
        const QSizeF fitted = picture.scaled(QSizeF(output), Qt::KeepAspectRatio);
        cb->setGraphicsPipeline(m_pipeline.get());
        cb->setViewport(QRhiViewport(float((output.width() - fitted.width()) / 2),
                                     float((output.height() - fitted.height()) / 2), float(fitted.width()),
                                     float(fitted.height())));
        cb->setShaderResources();
        cb->draw(4);
    }
    cb->endPass();
}

void VideoWidget::releaseResources()
{
    m_pipeline.reset();
    m_bindings.reset();
    for (auto &plane : m_planes)
        plane.reset();
    m_params.reset();
    m_sampler.reset();
    m_rhi = nullptr;
    m_uploadNeeded = m_frame != nullptr;
}

} // namespace fp
