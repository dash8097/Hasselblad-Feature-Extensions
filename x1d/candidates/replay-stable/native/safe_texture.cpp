#include "safe_texture.h"
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtCore/qpointer.h>

namespace {
class Uploaded final : public QSGTexture {
public:
    Uploaded(QOpenGLContext *c,unsigned id,const QSize &size,const X1D::FullToken &p)
        : context(c),name(id),dimensions(size),permit(p) {}
    ~Uploaded() override {
        QOpenGLContext *c=QOpenGLContext::currentContext();
        // Qt 正常销毁位于所属渲染上下文；上下文已消失时纹理由 GL 释放。
        if (name && c && context && (c==context || QOpenGLContext::areSharing(c,context))) {
            c->functions()->glDeleteTextures(1,&name);
            c->functions()->glFinish();
        } else if (name && context && context->isValid() && permit) {
            // 异常的跨上下文析构：继续占有额度，不能把未回收的 GPU 内存算作空闲。
            const auto held=permit;
            QObject::connect(context.data(),&QObject::destroyed,[held]() {});
        }
    }
    int textureId() const override { return int(name); }
    QSize textureSize() const override { return dimensions; }
    bool hasAlphaChannel() const override { return false; }
    bool hasMipmaps() const override { return false; }
    void bind() override {
        QOpenGLContext *c=QOpenGLContext::currentContext();
        if (c && context && (c==context || QOpenGLContext::areSharing(c,context))) {
            c->functions()->glBindTexture(GL_TEXTURE_2D,name);
            updateBindOptions();
        }
    }
private:
    QPointer<QOpenGLContext> context;
    unsigned name;
    QSize dimensions;
    X1D::FullToken permit; // CPU 清理后仍持有，直至这份 GPU 资源也归还。
};
}

QSGTexture *X1D::uploadChecked(const QImage &image,const FullToken &permit) {
    QOpenGLContext *c=QOpenGLContext::currentContext();
    if (!c || !c->isValid() || image.isNull() || image.format()!=QImage::Format_RGB32 ||
        image.bytesPerLine()!=image.width()*4) return nullptr;
    QOpenGLFunctions *f=c->functions();
    if (!f) return nullptr;
    int maximum=0;
    f->glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    if (image.width()>maximum || image.height()>maximum) return nullptr;
    const bool bgra=c->hasExtension(QByteArrayLiteral("GL_EXT_bgra")) ||
        c->hasExtension(QByteArrayLiteral("GL_EXT_texture_format_BGRA8888")) ||
        c->hasExtension(QByteArrayLiteral("GL_IMG_texture_format_BGRA8888"));
    if (!bgra) return nullptr; // 不临时复制另一张 Full 做颜色通道转换。
    // 不继承别的调用遗留错误；最多清八项，异常状态拒绝进入上传。
    unsigned error=GL_NO_ERROR;
    for (int i=0;i<8;++i) { error=f->glGetError(); if (error==GL_NO_ERROR) break; }
    if (error!=GL_NO_ERROR) return nullptr;
    int oldBinding=0,oldAlignment=0;
    f->glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldBinding);
    f->glGetIntegerv(GL_UNPACK_ALIGNMENT,&oldAlignment);
    unsigned texture=0;
    f->glGenTextures(1,&texture);
    if (!texture || f->glGetError()!=GL_NO_ERROR) {
        if (texture) f->glDeleteTextures(1,&texture);
        return nullptr;
    }
    f->glBindTexture(GL_TEXTURE_2D,texture);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    f->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    // 与固定 Qt 的 GLES BGRA8888 分支一致，无缩图和 mipmap 扩大。
    const unsigned bgra8888=0x80e1; // GL_BGRA_EXT，固定 GLES 公共头只声明扩展名称。
    f->glTexImage2D(GL_TEXTURE_2D,0,bgra8888,image.width(),image.height(),0,bgra8888,GL_UNSIGNED_BYTE,image.constBits());
    bool ok=f->glGetError()==GL_NO_ERROR;
    f->glPixelStorei(GL_UNPACK_ALIGNMENT,oldAlignment);
    f->glBindTexture(GL_TEXTURE_2D,unsigned(oldBinding));
    if (!ok) { f->glDeleteTextures(1,&texture); return nullptr; }
    try { return new Uploaded(c,texture,image.size(),permit); }
    catch (...) { f->glDeleteTextures(1,&texture); throw; }
}
