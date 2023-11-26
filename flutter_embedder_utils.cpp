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
#include <sstream>
#include <pthread.h>
#include <QMouseEvent>
#include <QtCore>
#include <typeinfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

FlutterEmbedderUtils::FlutterEmbedderUtils(QOpenGLContext *glWidget, QWindow *gWindow) : mRender(glWidget),
                                                                                         mQwindow(gWindow) {
    std::cout << "对象已创建" << std::endl;

    qRegisterMetaType<FlutterTask>("FlutterTask");
    connect(this, &FlutterEmbedderUtils::OnNewTask, this,
            &FlutterEmbedderUtils::HandleTask, Qt::QueuedConnection);
}

const int kRenderThreadIdentifer = 1;
const int kPlatformThreadIdentifer = 2;

void flutterPlatformMessageCallback(
        const FlutterPlatformMessage *message,
        void *user_data) {
    printf("channel: %s\n", message->channel);
    QByteArray bytedata((char *) message->message, message->message_size);
    QJsonDocument doc = QJsonDocument::fromJson(bytedata);
    QString str = QString(doc.toJson());
    printf("message: %s\n", str.toStdString().c_str());
}

void FlutterEmbedderUtils::initByWindow() {
    FlutterRendererConfig config = {};
    config.type = kOpenGL;
    config.open_gl.struct_size = sizeof(config.open_gl);
    config.open_gl.make_current = [](void *userdata) -> bool {
        FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        viewManager->mRender->makeCurrent(viewManager->mQwindow);
        return true;
    };
    config.open_gl.clear_current = [](void *userdata) -> bool {
        FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        viewManager->mRender->doneCurrent();
        return true;
    };
    config.open_gl.make_resource_current = [](void *userdata) -> bool {
        FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        return viewManager->RunsTasksOnSelfThread();
    };
    config.open_gl.present_with_info =
            [](void *userdata, const FlutterPresentInfo *info) -> bool {
                FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
                viewManager->mRender->swapBuffers(viewManager->mQwindow);
                return true;
            };

    config.open_gl.fbo_with_frame_info_callback =
            [](void *userdata, const FlutterFrameInfo *frameInfo) -> uint32_t {
                return 0;
            };
    config.open_gl.fbo_reset_after_present = true;
    config.open_gl.gl_proc_resolver = [](void *userdata,
                                         const char *procName) -> void * {
        FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        QOpenGLContext *context = viewManager->mRender;
        return (void *) context->getProcAddress(procName);
    };
    FlutterTaskRunnerDescription platform_task_runner = {};
    platform_task_runner.struct_size = sizeof(FlutterTaskRunnerDescription);
    platform_task_runner.user_data = this;
    platform_task_runner.runs_task_on_current_thread_callback =
            [](void *userdata) {
                FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
                return viewManager->RunsTasksOnSelfThread();
            };
    platform_task_runner.post_task_callback = [](FlutterTask task,
                                                 uint64_t target_time_nanos,
                                                 void *userdata) {
        FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        return viewManager->PostTask(task);
    };
    platform_task_runner.identifier = kPlatformThreadIdentifer;
    FlutterTaskRunnerDescription render_task_runner = {};
    render_task_runner.struct_size = sizeof(FlutterTaskRunnerDescription);
    render_task_runner.user_data = this;
    render_task_runner.runs_task_on_current_thread_callback =
            [](void *userdata) -> bool {
                FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
                return viewManager->RunsTasksOnSelfThread();
            };
    render_task_runner.post_task_callback = [](FlutterTask task,
                                               uint64_t target_time_nanos,
                                               void *userdata) {
        FlutterEmbedderUtils *viewManager = reinterpret_cast<FlutterEmbedderUtils *>(userdata);
        return viewManager->PostTask(task);
    };
    render_task_runner.identifier = kRenderThreadIdentifer;

    FlutterCustomTaskRunners custom_task_runners = {};
    custom_task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
    custom_task_runners.platform_task_runner = &platform_task_runner;
    custom_task_runners.render_task_runner = &render_task_runner;
    custom_task_runners
            .thread_priority_setter = [](FlutterThreadPriority priority) {
        switch (priority) {
            case FlutterThreadPriority::kBackground: {
                QThread::currentThread()->setPriority(QThread::LowPriority);
                break;
            }
            case FlutterThreadPriority::kDisplay: {
                QThread::currentThread()->setPriority(QThread::HighPriority);
                break;
            }
            case FlutterThreadPriority::kRaster: {
                QThread::currentThread()->setPriority(QThread::HighestPriority);
                break;
            }
            case FlutterThreadPriority::kNormal: {
                // For normal or default priority we do not need to set the
                // priority class.
                break;
            }
        }
    };

    FlutterProjectArgs args = {};
    FlutterEngineResult result;
    args.struct_size = sizeof(FlutterProjectArgs);
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
    args.custom_task_runners = &custom_task_runners;
    args.platform_message_callback = flutterPlatformMessageCallback;

    if (FlutterEngineRunsAOTCompiledDartCode()) {
        throw std::runtime_error("Not support AOT yet");
    } else {
        printf("JIT mode\n");
    }

    result = FlutterEngineInitialize(FLUTTER_ENGINE_VERSION, &config, &args,
                                     this, &mEngine);
    if (result != kSuccess || mEngine == nullptr) {
        printf("FATAL ERROR FlutterEngineInitialize %d %p", result, mEngine);
    }

}

// 将Flutter任务在主线程执行
void FlutterEmbedderUtils::PostTask(FlutterTask task) {
    if (QThread::currentThread() == thread()) {
        HandleTask(task);
    } else {
        emit OnNewTask(task);
    }
}

/// 判断是否主线程
bool FlutterEmbedderUtils::RunsTasksOnSelfThread() {
    return QThread::currentThread() == thread();
}


void FlutterEmbedderUtils::run() {
    FlutterEngineResult result = FlutterEngineRunInitialized(mEngine);
    if (result != kSuccess) {
        printf("FlutterEngineInitialize failed. result: %d engine: %p \n", result, mEngine);
    } else {
        printf("Flutter engine is running!");
        mIsRunning = true;
        HandleWindowResize();
    }
}

bool FlutterEmbedderUtils::HandleWindowResize() {
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

void FlutterEmbedderUtils::HandleTask(FlutterTask task) {
    if (!mEngine) {
        printf("engine not work\n");
        return;
    }
    if (FlutterEngineRunTask(mEngine, &task) != kSuccess) {
        printf("Could not post an engine task.\n");
    }
}

double FlutterEmbedderUtils::devicePixelRatio() {
    double p = mQwindow->devicePixelRatio();
//    printf("pixel: %f ,change: %f\n", p, p == 0.0 ? 1.0f : p);
    return p == 0.0 ? 1.0f : p;
}

bool FlutterEmbedderUtils::mouseEvent(QEvent *event) {
    if (!isRunning()) {
        printf("flutter not running\n");
        return false;
    }

    if (typeid(*event) == typeid(QResizeEvent)) {
        return HandleWindowResize();
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
        if (flutterEvent.phase != NULL) {
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
