

## 运行步骤

1. （必填）配置CMake环境变量 `QT_PATH`

由于QT的引用的包太大了，我没有上传到我的工程中，需要配置CMake环境变量`QT_PATH`，我的工程用的qt5

比如，命令行中输入：
```
export QT_PATH=/usr/local/Cellar/qt/6.6.0
```

qt下载请自行查找，macOS上下载可以通过 `brew install qt6`

2. （必填）配置CMake环境变量 `FLUTTER_ENGINE_LIB_PATH`

配置FlutterEmbedder.framework地址，比如：
```
export FLUTTER_ENGINE_LIB_PATH=/myCode/flutter/engine/src/out/mac_debug_unopt
```

3. （必填）配置编译环境变量 `FLUTTER_ASSETS`

