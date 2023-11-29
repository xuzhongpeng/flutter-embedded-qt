
## 本机环境

> CMake 3.22
> QT Framework 6.6
> Flutter 3.0.2
> ninja 1.11.1 或者 make 14.0.3

## 运行步骤

1. （必填）配置CMake环境变量 `QT_PATH`

由于QT的引用的包太大了，我没有上传到我的工程中，需要配置CMake环境变量`QT_PATH`，我的工程用的QT6

比如，命令行中输入：

```
export QT_PATH=/usr/local/Cellar/qt/6.6.0
```

qt下载请自行查找，macOS上下载可以通过 `brew install qt6`

如果想用`Qt5`，那么需要将CMakeLists.txt中的`Qt6`都改为`Qt5`

2. （必填）配置FlutterEmbedder地址 `FLUTTER_ENGINE_LIB_PATH`

配置FlutterEmbedder.framework地址，比如：

```
export FLUTTER_ENGINE_LIB_PATH=/myCode/flutter/engine/src/out/mac_debug_unopt/FlutterEmbedder.framework
```

3. （必填）配置编译环境变量 `FLUTTER_ASSETS`地址

`FLUTTER_ASSETS`地址是本地编译好的`Debug`版本的Flutter工程资源，编译Debug版本可以通过`flutter build bundle`进行，会在项目下生成`build/flutter_assets`文件。然后配置`FLUTTER_ASSETS`.

```
export FLUTTER_ASSETS=~/flutter_sample/build/flutter_assets
```


## 编译及运行
可以使用ninja或者make进行编译，如使用make如下

```
mkdir build && cd build
cmake ..
make .
```

运行build下的`flutter_embedder_qt`

```
./flutter_embedder_qt
```