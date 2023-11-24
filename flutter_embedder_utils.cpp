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

FlutterEmbedderUtils::FlutterEmbedderUtils(QOpenGLContext *glWidget, QWindow *gWindow) : mRender(glWidget),
                                                                                         mQwindow(gWindow) {
    std::cout << "对象已创建" << std::endl;

    qRegisterMetaType<FlutterTask>("FlutterTask");
    connect(this, &FlutterEmbedderUtils::OnNewTask, this,
            &FlutterEmbedderUtils::HandleTask, Qt::QueuedConnection);
}
const int kRenderThreadIdentifer = 1;
const int kPlatformThreadIdentifer = 2;
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
    const char* envValue = std::getenv("FLUTTER_ASSETS");
    if(envValue == nullptr) {
#ifdef PROJECT_ROOT_DIR
        flutter_assets = PROJECT_ROOT_DIR + std::string("/lib/flutter_assets"); // 定义在cmakelist中
#endif
    } else {
        flutter_assets = envValue;
    }
    printf("flutter assets path: %s\n", flutter_assets.c_str());
    args.assets_path = flutter_assets.c_str();
#ifdef FLUTTER_ICUDTL_PATH
    args.icu_data_path = FLUTTER_ICUDTL_PATH; // 定义在cmakelist中
#endif
    args.custom_task_runners = &custom_task_runners;

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

int FlutterEmbedderUtils::HandleWindowResize() {
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
    FlutterEngineSendWindowMetricsEvent(mEngine, &event);
    return 0;
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
        HandleWindowResize();
        return true;
    } else {
        // 鼠标事件
        FlutterPointerEvent flutterEvent = {};
        flutterEvent.struct_size = sizeof(FlutterPointerEvent);
        flutterEvent.device_kind = kFlutterPointerDeviceKindMouse;
        if (typeid(*event) == typeid(QMouseEvent)) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            flutterEvent.x = mouseEvent->pos().x() * devicePixelRatio();
            flutterEvent.y = mouseEvent->pos().y() * devicePixelRatio();
            if (mouseEvent->button() == Qt::LeftButton) {
                flutterEvent.buttons = kFlutterPointerButtonMousePrimary;
            } else if (mouseEvent->button() == Qt::RightButton) {
                flutterEvent.buttons = kFlutterPointerButtonMouseSecondary;
            } else if (mouseEvent->button() == Qt::MiddleButton) {
                flutterEvent.buttons = kFlutterPointerButtonMouseMiddle;
            }
        }
        flutterEvent.timestamp =
                std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count();
        switch (event->type()) {
            case QEvent::MouseButtonPress: //2
                if (!hasRemove) {
                    flutterEvent.phase = FlutterPointerPhase::kDown;
                    mouseDown = true;
                }
                break;
            case QEvent::MouseButtonRelease://3
                if (!hasRemove && mouseDown) {
                    flutterEvent.phase = FlutterPointerPhase::kUp;
                    mouseDown = false;
                }
                break;
            case QEvent::MouseMove://13
                if (!hasRemove) {
                    if (mouseDown) {
                        flutterEvent.phase = FlutterPointerPhase::kMove;
                    } else {
                        flutterEvent.phase = FlutterPointerPhase::kHover;
                    }
                }
                break;
            case QEvent::Enter://10
                if (hasRemove) {
                    flutterEvent.phase = FlutterPointerPhase::kAdd;
                    hasRemove = false;
                }
                break;
            case QEvent::HoverMove://129
                flutterEvent.phase = FlutterPointerPhase::kHover;
                break;
            case QEvent::Leave://11
            case QEvent::FocusOut://9
                if (!hasRemove) {
                    if (mouseDown) {
                        mouseDown = false;
                        flutterEvent.phase = FlutterPointerPhase::kUp;
                        FlutterEngineSendPointerEvent(mEngine, &flutterEvent, 1);
                    }
                    flutterEvent.phase = FlutterPointerPhase::kRemove;
                    hasRemove = true;
                }
                break;
            default:break;
        }
        if (flutterEvent.phase != NULL) {
            FlutterEngineResult result = FlutterEngineSendPointerEvent(mEngine, &flutterEvent, 1);
            return result == kSuccess;
        }
    }
    return false;
}
