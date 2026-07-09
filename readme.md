# FractalDumpset

我是为 L-Model 团队编写文档的 Codex 工具人。这个文件用于记录当前仓库已经存在的代码文件分工、类和功能进度。FractalDumpset 当前是一个 Windows + OpenGL 3.3 + C++17 图形实验工程，已经包含 Win32 窗口与 OpenGL context、Enola2 组件树、事件分发、组件离屏 FBO 渲染、OBJ/MTL 加载、贴图加载、网格绘制、Blender 风格相机控制、场景容器、双击拾取测试和一个 SceneEditor demo 骨架。

当前主要运行链路是 `main.cpp -> MyRootComponent -> SceneEditor -> GridModel / SceneTest1 / ObjModel zeraora`。当前主要渲染链路是 `rootComponent.DoRender()` 进入组件系统，由 `RenderContext2` 创建组件 FBO，随后 `SceneEditor` 绘制网格、测试场景和可选 gizmo，再由组件系统 blit 回默认 framebuffer。

## 根目录

`CMakeLists.txt` 定义了 `FractalDumpset` 可执行目标，使用 C++17，编译 `Source/main.cpp` 和 `Source/External/glad/src/glad.c`，链接 OpenGL、`dwmapi.lib` 和 `uxtheme.lib`，并在 Release 配置中保留了 MSVC / MinGW 下的体积优化和链接优化选项。

`Resources/` 是当前资源目录，当前 demo 使用其中的 `Resources/zeraora/zeraora.obj` 和 `Resources/zeraora/zeraora.mtl`，MTL 中引用的 diffuse texture 会通过 `ImageLoader` 加载为 OpenGL texture。

`ResourcesPacker/` 和 `ResourcesPacker.exe` 是资源打包工具源码目录与可执行文件，`Resources_Resources.rsh` 是已经生成的资源打包头文件，`App.h` 中保留了 `USE_PACK_RESOURCES` 条件路径用于资源解包和清理。

`TODOlists.md` 是当前开发备忘文件，里面记录了将 `Model` 和 `Scene` 进一步合并为 `ModelComponent`，并加入子模型、tick、transform、shader program 等接口的后续想法。

## 平台入口

`Source/main.cpp` 是 Win32 平台入口文件，负责创建主窗口、注册支持双击的窗口类、设置 pixel format、创建 OpenGL 3.3 core profile context、加载 glad、初始化 DWM flush、创建全局 `MyRootComponent rootComponent`，并在主循环中处理窗口消息、调用 `rootComponent.DoRender()` 和执行 `SwapBuffers`。

`main.cpp` 目前将 Win32 消息转换为 Enola2 事件，包括 resize、键盘按下与抬起、鼠标移动、左右键按下、中键按下与抬起、滚轮、左键双击、右键双击、关闭窗口和销毁窗口。窗口 resize 会更新根组件 bounds，ESC 会退出主循环，退出阶段会释放 OpenGL context 和 DC。

## Enola2 组件与事件

`Source/Enola2Component.h` 是当前组件系统文件，包含 `Rectf`、`RenderContext`、`RenderContext2` 和 `Component`。这部分代码提供了类似 JUCE 的组件树、bounds 管理、父子组件递归、离屏 framebuffer 绘制和全局坐标到组件局部坐标的转换。

`Enola2::Rectf` 是矩形数据结构，保存 `x`、`y`、`w`、`h` 四个浮点字段，用于组件 bounds 和渲染区域描述。

`Enola2::RenderContext` 是早期基础渲染上下文辅助类，负责重置 OpenGL 状态，并在 `StartRender()` 中设置 viewport 和 scissor，在 `EndRender()` 中恢复基础状态。

`Enola2::RenderContext2` 是当前组件渲染实际使用的离屏 FBO 上下文，内部维护 framebuffer、color texture、depth texture 和当前组件 bounds。它会按组件尺寸创建或复用 target，在组件本地坐标中设置干净的 viewport/scissor，并在渲染结束后把组件 FBO blit 回默认 framebuffer 对应区域。

`Enola2::Component` 是组件基类，保存父组件、子组件列表、相对父组件的 bounds 和全局坐标 `directX/directY`。它提供 `AddChild`、`SetBounds`、`GetBounds`、`GetLocalBounds`、`GlobalPosToComponent`，并通过 `DoInit/Init`、`DoResize/Resize`、`DoRender/Render` 形成递归式生命周期，其中 `DoRender` 会为当前组件申请 FBO 并调用用户覆写的 `Render(GLuint frameBuffer)`。

`Source/Enola2Event.h` 是当前事件系统文件，包含鼠标状态、键盘状态、鼠标事件、键盘事件和全局事件监听基类。Win32 事件会在 `main.cpp` 中转换成这里的数据结构并分发。

`Enola2::MouseState` 定义鼠标事件状态，目前包含空状态、移动、左键按下、右键按下、左键双击、右键双击、滚轮、中键按下和中键抬起。

`Enola2::KeyState` 定义键盘事件状态，目前包含空状态、按下和抬起。

`Enola2::MouseEvent` 保存鼠标事件的 `x`、`y`、`wheel` 和 `state`，用于传递鼠标位置、滚轮值和鼠标动作类型。

`Enola2::KeyEvent` 保存键盘事件的 `key` 和 `state`，用于传递按键编码和按下/抬起状态。

`Enola2::EventListener` 是全局事件监听基类，提供 `OnMouse`、`OnKey`、`Register`、`Unregister`、`DispatchMouse` 和 `DispatchKey`。它内部使用静态 listener 列表广播事件，析构时会自动注销当前监听对象。

## E3d 图形层

`Source/E3dShader.h` 是 GLSL 编译工具文件，目前只包含 `ShaderCompiler`。这个类提供静态 `CompileShaderGLSL`，负责从字符串编译 vertex shader 和 fragment shader，链接 shader program，输出编译或链接错误，并返回 OpenGL program id。

`Source/E3dUtils.h` 是 E3d 工具文件，包含调试日志、图片加载和视角控制。这里的功能目前服务于 OBJ/MTL 贴图读取和 SceneEditor 中的 Blender 风格相机控制。

`E3dUtils::LogPrintf` 是调试日志函数，按 DEBUG 条件输出格式化日志。

`ImageLoader` 是图片加载类，提供 `LoadTexture2D`。它使用 stb_image 读取图片文件，根据通道数选择 OpenGL format，创建 2D texture，上传像素数据，生成 mipmap，设置 wrap/filter，并返回 texture id。

`ViewController` 是一个 Blender 风格视角控制类，继承自 `Enola2::EventListener`。它保存 camera 的 position、target、up 和 view matrix，处理中键拖动 orbit、Shift + 中键拖动 pan、滚轮 zoom 和 Shift 键状态，最后通过 `GetNowView` 与 `GetNowCameraPos` 提供鼠标键盘操作后的视角矩阵和相机位置。

`Source/E3dModel.h` 是当前 3D 模型文件，包含 `Model`、`ObjModel` 和 `GridModel`。这里保存了基础模型 transform、OBJ/MTL 加载、VAO/VBO 创建、材质分组绘制、屏幕点拾取和网格绘制功能。

`Model` 是 3D 模型基类，内部保存 `glm::mat4 model`，提供 `Init`、`Draw`、`ResetModel`、`GetModel`、`SetModel`、`SetModelPos`、`TranslateModel`、`ScaleModel`、`RotateModel` 和 `IsPointToModel`。它当前承担可变换、可绘制、可拾取对象的基础接口。

`ObjModel` 是 OBJ/MTL 模型类，继承自 `Model`。它内部包含 `MtlGroup`、`FaceGroup`、`FaceIndex`、`Vertex` 和 `RenderGroup`，能够读取 OBJ 顶点、法线、UV、三角面和材质分组，读取 MTL 的基础材质参数和 diffuse texture，创建 VAO/VBO，按材质组绘制，并上传模型矩阵、视图矩阵、投影矩阵、相机位置、材质颜色、透明度和 diffuse texture 相关 uniform。

`ObjModel` 还实现了拾取相关功能。它提供 `RayTriangleIntersect` 做射线三角形相交测试，`IsPointToModel` 会将屏幕点转换成世界射线，再转换到模型局部空间遍历三角形，最后返回最近命中点的相机深度。

`GridModel` 是 3D 网格模型类，继承自 `Model`。它内部包含 `GridVertex`，可以生成 XZ 平面网格顶点、普通网格线索引、X 轴索引和 Z 轴索引，创建 VAO/VBO/EBO，创建淡出纹理，根据相机位置更新淡出 UV，并绘制普通网格线、红色 X 轴和蓝色 Z 轴。

`GridModel` 在绘制时会上传 `uModel`、`uView`、`uProjection`、`uNormalMatrix` 和基础材质 uniform，绑定淡出纹理为 diffuse map，启用 alpha blend。析构时会释放 grid VBO、EBO、VAO 和 fade texture。

`Source/E3dScene.h` 是当前场景与编辑器 demo 文件，包含 `Scene`、`TransformGizmo`、`SceneTest1` 和 `SceneEditor`。这里把模型容器、测试场景、gizmo 占位和当前 3D 编辑器组件串在一起。

`Scene` 是场景容器类，继承自 `Model`。它内部包含 `TagModel`，保存命名模型列表、空模型和空 tag model，提供 `AddModel`、`GetModelRef`、`Draw`、`IsPointToModel` 和 `GetPointToModelRef`，能够添加命名模型、按名称获取模型、递归绘制子模型、遍历子模型执行拾取，并返回最近命中的命名模型记录。

`TransformGizmo` 是变换操纵器类，继承自 `Scene`。它目前是 gizmo 功能的占位类型，已经建立类和继承关系，还没有填入具体操纵器绘制与交互逻辑。

`SceneTest1` 是测试场景类，继承自 `Scene`。它内部保存 `ObjModel zeraora`，在 `Init` 中加载 `Resources/zeraora/zeraora.obj` 和 `Resources/zeraora/zeraora.mtl`，创建 VAO/VBO，并把模型以 `zeraora1` 的名称加入场景。

`SceneEditor` 是当前主要 3D 编辑器 demo 组件，继承自 `Enola2::Component` 和 `Enola2::EventListener`。它内部包含一组内嵌 vertex/fragment shader 字符串、`ViewController`、`GridModel`、`SceneTest1`、`TransformGizmo`、gizmo 开关、shader program、fov 和 projection matrix，负责初始化相机、场景、网格和 shader，并在组件 FBO 中绘制当前 3D 视图。

`SceneEditor` 当前已经实现 resize 时更新 perspective projection、Render 中绘制 grid 和 scene、可选绘制 gizmo、左键双击时将全局鼠标坐标转换为组件局部坐标、对场景执行拾取，并打印命中的模型名称。

## 当前应用层

`Source/App.h` 是当前应用层文件，包含早期 `TestObjModel` 和当前根组件 `MyRootComponent`。当前运行使用 `MyRootComponent`，其内部挂载 `SceneEditor`。

`TestObjModel` 是早期 OBJ 测试组件，继承自 `Enola2::Component`。它内部包含内嵌标准 mesh shader、`ViewController`、`ShaderCompiler`、`GridModel` 和 `ObjModel`，可以加载 zeraora OBJ/MTL、创建 grid、编译 shader、设置相机、每帧旋转 OBJ，并绘制 grid 与 OBJ；当前类保留但根组件不直接使用。

`MyRootComponent` 是当前根组件，继承自 `Enola2::Component` 和 `Enola2::EventListener`。它内部包含 `SceneEditor v3d`，构造时添加为子组件，并在 `Resize` 中将 SceneEditor 放置到窗口中间区域；同时保留了 `USE_PACK_RESOURCES` 条件下的资源解包和清理逻辑。

## 旧测试文件

`Source/Shader.h` 是早期 shader 文件加载类文件，包含 `Shader`。这个类保存 `GLuint ID`，可以从 vertex/fragment 文件路径读取源码，编译 shader，链接 program，通过 `Use` 激活 program，通过 `SetMat4` 上传 mat4 uniform，并输出编译或链接错误。

`Source/ObjComponent.h` 是早期 OBJ 组件文件，包含 `ObjComponent`。这个类可以加载 OBJ 顶点和法线，支持 Blender 风格 `v//n` face，构建 mesh，创建 VAO/VBO/EBO，绘制 indexed triangles，保存 position、rotation、scale，并提供 `Reset`、`Move`、`Rotate`、`Scale` 和 `GetModelMatrix`。

`Source/Test3DObj.h` 是早期 3D OBJ 与景深测试文件，包含 `BoundHighlighting` 和 `View3DWorld`。这些代码保留了 OBJ 显示、grid 生成、边框高亮、离屏 FBO、depth texture、fullscreen quad 和 DOF 后处理实验。

`BoundHighlighting` 是绿色边框高亮组件，继承自 `Enola2::Component`。它创建简单 shader 和屏幕空间矩形 VAO/VBO，在组件 FBO 中绘制线框矩形，resize 时输出 bounds，析构时释放 GL 资源。

`View3DWorld` 是早期 3D 世界测试组件，继承自 `Enola2::Component`。它内部包含 `Shader`、`ObjComponent`、`BoundHighlighting`、相机矩阵、grid buffer、DOF FBO、color/depth texture、后处理 shader 和 fullscreen quad，可以加载 OBJ、绘制 grid、绘制模型、执行景深后处理，并绘制边界高亮子组件。

`Source/TestTerrain.h` 是早期程序化地形测试文件，包含 `PerlinNoise2D`、全局 shader helper 和 `World`。这里保留了地形高度图生成、地形网格、地形 shader、相机控制和景深后处理实验。

`PerlinNoise2D` 是噪声类，提供 Perlin noise、FBM 和 ridged noise，用于生成程序化地形高度数据。

`CompileShader` 和 `CreateShaderProgram` 是 `TestTerrain.h` 中的全局 shader helper，用于编译 shader 和链接 shader program。

`World` 是程序化地形测试组件，继承自 `Enola2::Component` 和 `Enola2::EventListener`。它内部包含地形 shader、DOF shader、高度图、terrain VAO/VBO/EBO、相机参数、鼠标键盘状态和时间变量，可以生成并上传高度图、生成地形网格、绘制地形、执行 depth-aware blur 后处理，并处理 Blender 风格相机 orbit、pan 和 zoom。

## Loader 与第三方代码

`Source/GLLoader.h` 是 OpenGL loader 头文件，当前只引入 glad。

`Source/stb_image.h` 是图片读取库，当前由 `ImageLoader` 使用。

`Source/External/glad` 是 OpenGL 函数加载代码，当前由 `main.cpp` 初始化并由 `GLLoader.h` 间接提供 OpenGL API。

`Source/External/glm` 是数学库，当前用于向量、矩阵、投影、相机、transform、射线和几何计算。

## 当前进度

当前已经完成 Win32 窗口、OpenGL context、glad 加载、主循环、组件树、组件离屏 FBO 渲染、组件 init/resize/render 递归、基础事件系统、鼠标键盘双击事件接入、Blender 风格相机控制、GLSL 字符串编译、图片加载为 texture、OBJ 加载、MTL 加载、diffuse texture 绑定、OBJ VAO/VBO 创建、网格模型、场景容器、测试场景、SceneEditor demo 骨架和双击拾取测试。

当前已经建立但仍处于占位或后续扩展阶段的部分包括 `TransformGizmo`、资源打包运行路径、`Model` 与 `Scene` 合并为 `ModelComponent` 的方向、场景持久化和更完整的 shader/pass 工作流。
