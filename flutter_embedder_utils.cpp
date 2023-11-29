//
// Created by xzp on 2023/11/16.
//

#include "flutter_embedder_utils.h"
#include <cstdlib>
#include <iostream>
#include <QThread>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <thread>
#include <QMouseEvent>
#include <QtCore>
#include <typeinfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

FlutterEmbedderUtils::FlutterEmbedderUtils(QOpenGLContext *context, QWindow *window) : mContext(context),
                                                                                       mQwindow(window) {
    // 注册FlutterTask自定义类型到Qt的元对象系统以便可以在信号和槽中使用。
    // 这是必需的，因为FlutterTask不是一个Qt内置类型。
    qRegisterMetaType<FlutterTask>("FlutterTask");
    // 连接handleMainTask信号到handleTask槽。
    // 当handleMainTask信号被触发时，handleTask槽将被调用。
    connect(this, &FlutterEmbedderUtils::handleMainTask, this,
            &FlutterEmbedderUtils::handleTask, Qt::QueuedConnection);
}

const int kRenderThreadIdentifer = 1;
const int kPlatformThreadIdentifer = 2;

void flutterPlatformMessageCallback(
        const FlutterPlatformMessage *message,
        void *user_data) {
//    printf("channel: %s\n", message->channel);
    QByteArray bytedata((char *) message->message, message->message_size);
    QJsonDocument doc = QJsonDocument::fromJson(bytedata);
    QString str = QString(doc.toJson());
//    printf("message: %s\n", str.toStdString().c_str());
}

void FlutterEmbedderUtils::run() {
    // 渲染模式相关配置
    FlutterRendererConfig config = {};
    // 设置OpenGL渲染
    config.type = kOpenGL;
    config.open_gl.struct_size = sizeof(config.open_gl);
    // OpenGL渲染上下文，将Flutter里的OpenGL操作都绑定到mQwindow中
    config.open_gl.make_current = [](void *userdata) -> bool {
        FlutterEmbedderUtils *host = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        host->mContext->makeCurrent(host->mQwindow);
        return true;
    };
    // 设置clear_current回调，此回调在需要解除当前渲染上下文时调用
    config.open_gl.clear_current = [](void *userdata) -> bool {
        FlutterEmbedderUtils *host = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        // 使用自定义的渲染器来清除当前的OpenGL上下文
        host->mContext->doneCurrent();
        return true;
    };
    // 设置资源上下文的回调，此回调在需要设置资源加载上下文时调用
    config.open_gl.make_resource_current = [](void *userdata) -> bool {
        FlutterEmbedderUtils *host = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        // 在这里，我们检查是否在相同的线程上运行任务
        return host->runsTasksOnSelfThread();
    };
    // 设置present_with_info回调，此回调在需要将渲染好的帧展示到屏幕上时调用
    config.open_gl.present_with_info =
            [](void *userdata, const FlutterPresentInfo *info) -> bool {
                FlutterEmbedderUtils *host = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
                // 使用自定义的渲染器来交换帧缓冲区，展示新的帧
                host->mContext->swapBuffers(host->mQwindow);
                return true;
            };
    // 设置用于获取当前帧缓冲对象的回调，这可以用于优化，例如在多层渲染中
    config.open_gl.fbo_with_frame_info_callback =
            [](void *userdata, const FlutterFrameInfo *frameInfo) -> uint32_t {
                return 0;
            };
    // 设置一个标志，表示在帧展示后帧缓冲对象是否应该被重置
    // 这通常用于OpenGL上下文需要在每次渲染后重置状态的场景
    config.open_gl.fbo_reset_after_present = false;
    // 设置一个函数，用于解析OpenGL函数的地址
    config.open_gl.gl_proc_resolver = [](void *userdata,
                                         const char *procName) -> void * {
        FlutterEmbedderUtils *host = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        return (void *) host->mContext->getProcAddress(procName);
    };
    FlutterTaskRunnerDescription render_task_runner = {};
    render_task_runner.struct_size = sizeof(FlutterTaskRunnerDescription);
    render_task_runner.user_data = this;
    // 提供一个回调函数，用于检查当前线程是否是渲染线程
    render_task_runner.runs_task_on_current_thread_callback =
            [](void *userdata) -> bool {
                FlutterEmbedderUtils *host = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
                return host->runsTasksOnSelfThread();
            };
    // 提供一个回调函数，用于将任务投递到渲染线程
    render_task_runner.post_task_callback = [](FlutterTask task,
                                               uint64_t target_time_nanos,
                                               void *userdata) {
        FlutterEmbedderUtils *host = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        // 将任务投递到宿主平台的渲染线程
        return host->postTask(task);
    };
    // 设置渲染任务运行器的唯一标识符，用于区别其它任务
    render_task_runner.identifier = kRenderThreadIdentifer;

    // 将自定义任务运行器组合起来
    FlutterCustomTaskRunners custom_task_runners = {};
    custom_task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
    // 将渲染任务运行器指定给自定义任务运行器
    custom_task_runners.render_task_runner = &render_task_runner;

    // 定义一个FlutterProjectArgs，用于最终传递给FlutterEngine
    FlutterProjectArgs args = {};
    args.struct_size = sizeof(FlutterProjectArgs);
    // 绑定自定义的任务运行器
    args.custom_task_runners = &custom_task_runners;
    std::string flutter_assets;
    const char *envValue = std::getenv("FLUTTER_ASSETS"); // ~/flutter_sample/build/flutter_assets
    if (envValue == nullptr) {
        throw std::runtime_error("Environment variable FLUTTER_ASSETS not set");
    } else {
        flutter_assets = envValue;
    }
    printf("flutter assets path: %s\n", flutter_assets.c_str());
    args.assets_path = flutter_assets.c_str();
#ifdef FLUTTER_ICUDTL_PATH
    args.icu_data_path = FLUTTER_ICUDTL_PATH; // 定义在cmakelist中
#endif
    args.platform_message_callback = flutterPlatformMessageCallback;

    if (FlutterEngineRunsAOTCompiledDartCode()) {
        throw std::runtime_error("Not support AOT yet");
    } else {
        printf("JIT mode\n");
    }

    FlutterEngineResult result = FlutterEngineRun(FLUTTER_ENGINE_VERSION, &config, &args,
                                                  this, &mEngine);
    if (result != kSuccess) {
        printf("FlutterEngineInitialize error: %d %p\n", result, mEngine);
    } else {
        printf("Flutter engine is running!\n");
        mIsRunning = true;
        handleWindowResize();
    }

}

/// 判断是否主线程
bool FlutterEmbedderUtils::runsTasksOnSelfThread() {
    return QThread::currentThread() == thread();
}

bool FlutterEmbedderUtils::handleWindowResize() {
    if (!isRunning()) {
        printf("flutter not running\n");
        return -1;
    }
    double pixelRatio = devicePixelRatio();
    printf("Do window resize %dx %d ratio=%f \n", mQwindow->size().width(), mQwindow->size().height(), pixelRatio);
    FlutterWindowMetricsEvent event = {};
    event.struct_size = sizeof(event);
    event.width = mQwindow->size().width() * pixelRatio;
    event.height = mQwindow->size().height() * pixelRatio;
    event.pixel_ratio = pixelRatio;
    FlutterEngineResult result = FlutterEngineSendWindowMetricsEvent(mEngine, &event);
    return result == kSuccess;
}

// postTask函数负责在主线程中调度一个Flutter任务
void FlutterEmbedderUtils::postTask(FlutterTask task) {
    // 检查是否当前线程是创建此对象的线程（即主线程）。
    if (QThread::currentThread() == thread()) {
        // 如果是在主线程，直接处理任务。
        handleTask(task);
    } else {
        // 如果不是在主线程，通过发射信号来请求主线程去处理任务。
        emit handleMainTask(task);
    }
}

// handleTask函数实际上在Flutter引擎中执行任务。
void FlutterEmbedderUtils::handleTask(FlutterTask task) {
    // 检查是否Flutter引擎实例是有效的。
    if (!mEngine) {
        // 如果引擎没有正确初始化，输出错误信息。
        printf("engine not work\n");
        return;
    }
    // 尝试在Flutter引擎中运行任务。
    if (FlutterEngineRunTask(mEngine, &task) != kSuccess) {
        // 如果任务不能被投递到Flutter引擎，输出错误信息。
        printf("Could not post an engine task.\n");
    }
}

double FlutterEmbedderUtils::devicePixelRatio() {
    double p = mQwindow->devicePixelRatio();
//    printf("pixel: %f ,change: %f\n", p, p == 0.0 ? 1.0f : p);
    return p == 0.0 ? 1.0f : p;
}

bool FlutterEmbedderUtils::flutterEvent(QEvent *event) {
    if (!isRunning()) {
        printf("flutter not running\n");
        return false;
    }

    if (typeid(*event) == typeid(QResizeEvent)) {
        return handleWindowResize();
    } else if (typeid(*event) == typeid(QKeyEvent)) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
//    FlutterKeyEvent flutterEvent = {};
//    flutterEvent.struct_size = sizeof(FlutterKeyEvent);
//    flutterEvent.timestamp = keyEvent->timestamp();
//
//    switch (keyEvent->type()) {
//      case QEvent::KeyPress:
//        flutterEvent.character = keyEvent->text().toStdString().c_str();
//        flutterEvent.type = FlutterKeyEventType::kFlutterKeyEventTypeDown;
//        printf("type: FlutterKeyEventType::kFlutterKeyEventTypeDown\n");
//        type = "keydown";
//        printf(flutterEvent.character);
//        break;
//      case QEvent::KeyRelease:
//        flutterEvent.type = FlutterKeyEventType::kFlutterKeyEventTypeUp;
//        printf("type: FlutterKeyEventType::kFlutterKeyEventTypeUp\n");
//        type = "keyup";
//        break;
//      default:
//        return false;
//    }
//    printf("type: %d\n", keyEvent->type());
//    flutterEvent.logical = keyEvent->key();
//    flutterEvent.physical = keyEvent->nativeScanCode();
//    printf("character: %s\n", flutterEvent.character);
//    return FlutterEngineSendKeyEvent(mEngine,
//                                     &flutterEvent,
//                                     [](bool handled, void *user_data) {
//
//                                     },
//                                     this) == kSuccess;
        std::string type;
        std::string text;
        switch (keyEvent->type()) {
            case QEvent::KeyPress:
//        printf("type: FlutterKeyEventType::kFlutterKeyEventTypeDown\n");
                type = "keydown";
                text = keyEvent->text().toStdString();
                break;
            case QEvent::KeyRelease:
//        printf("type: FlutterKeyEventType::kFlutterKeyEventTypeUp\n");
                type = "keyup";
                break;
            default:
                return false;
        }
        QJsonObject flutterEvent{
                {"keyCode",    keyEvent->key()},
                {"keymap",     "macos"},
                {"modifiers",  translateModifiers(keyEvent->modifiers())},
                {"type",       type.c_str()},
                {"characters", text.c_str()}};
        QByteArray keyData = QJsonDocument(flutterEvent).toJson(QJsonDocument::Compact);
        FlutterPlatformMessage keyMessage = {
                sizeof(FlutterPlatformMessage),
                "flutter/keyevent",
                (uint8_t *) keyData.data(),
                (size_t) keyData.size(),
                nullptr,
        };
        return FlutterEngineSendPlatformMessage(mEngine, &keyMessage) == kSuccess;
    } else {
        // 鼠标事件
        FlutterPointerEvent flutterEvent = {};
        flutterEvent.
                struct_size = sizeof(FlutterPointerEvent);
        flutterEvent.
                device_kind = kFlutterPointerDeviceKindMouse;
        if (typeid(*event) == typeid(QMouseEvent)) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            flutterEvent.
                    x = mouseEvent->pos().x() * devicePixelRatio();
            flutterEvent.
                    y = mouseEvent->pos().y() * devicePixelRatio();
            if (mouseEvent->

                    button()

                == Qt::LeftButton) {
                flutterEvent.
                        buttons = kFlutterPointerButtonMousePrimary;
            } else if (mouseEvent->

                    button()

                       == Qt::RightButton) {
                flutterEvent.
                        buttons = kFlutterPointerButtonMouseSecondary;
            } else if (mouseEvent->

                    button()

                       == Qt::MiddleButton) {
                flutterEvent.
                        buttons = kFlutterPointerButtonMouseMiddle;
            }
        }
        flutterEvent.
                timestamp =
                std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count();
        switch (event->

                type()

                ) {
            case QEvent::MouseButtonPress: //2
                if (!hasRemove) {
                    flutterEvent.
                            phase = FlutterPointerPhase::kDown;
                    mouseDown = true;
                }
                break;
            case QEvent::MouseButtonRelease://3
                if (!
                            hasRemove && mouseDown
                        ) {
                    flutterEvent.
                            phase = FlutterPointerPhase::kUp;
                    mouseDown = false;
                }
                break;
            case QEvent::MouseMove://13
                if (!hasRemove) {
                    if (mouseDown) {
                        flutterEvent.
                                phase = FlutterPointerPhase::kMove;
                    } else {
                        flutterEvent.
                                phase = FlutterPointerPhase::kHover;
                    }
                }
                break;
            case QEvent::Enter://10
                if (hasRemove) {
                    flutterEvent.
                            phase = FlutterPointerPhase::kAdd;
                    hasRemove = false;
                }
                break;
            case QEvent::HoverMove://129
                flutterEvent.
                        phase = FlutterPointerPhase::kHover;
                break;
            case QEvent::Leave://11
            case QEvent::FocusOut://9
                if (!hasRemove) {
                    if (mouseDown) {
                        mouseDown = false;
                        flutterEvent.
                                phase = FlutterPointerPhase::kUp;
                        FlutterEngineSendPointerEvent(mEngine, &flutterEvent,
                                                      1);
                    }
                    flutterEvent.
                            phase = FlutterPointerPhase::kRemove;
                    hasRemove = true;
                }
                break;
            default:
                break;
        }
        if (flutterEvent.phase) {
            FlutterEngineResult result = FlutterEngineSendPointerEvent(mEngine, &flutterEvent, 1);
            return result ==
                   kSuccess;
        }
    }
    return false;
}

static const std::map<int, int> kQtKeyModifierMap{{Qt::ShiftModifier,   0x0001},
                                                  {Qt::MetaModifier,    0x0002},
                                                  {Qt::AltModifier,     0x0004},
                                                  {Qt::ControlModifier, 0x0008},
                                                  {Qt::KeypadModifier,  0x0020}};

int FlutterEmbedderUtils::translateModifiers(Qt::KeyboardModifiers rawMods) {
    int modifiers = 0;
    if (rawMods & Qt::ShiftModifier) {
        modifiers |= kQtKeyModifierMap.at(Qt::ShiftModifier);
    }
    if (rawMods & Qt::ControlModifier) {
        modifiers |= kQtKeyModifierMap.at(Qt::ControlModifier);
    }
    if (rawMods & Qt::AltModifier) {
        modifiers |= kQtKeyModifierMap.at(Qt::AltModifier);
    }
    if (rawMods & Qt::MetaModifier) {
        modifiers |= kQtKeyModifierMap.at(Qt::MetaModifier);
    }
    if (rawMods & Qt::KeypadModifier) {
        modifiers |= kQtKeyModifierMap.at(Qt::KeypadModifier);
    }

    return modifiers;
}
