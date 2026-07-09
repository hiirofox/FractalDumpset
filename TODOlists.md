感觉model，scene还能再合并。合并成ModelComponent。
然后提供一些基础的组件，比如说：
AddChildModel(ModelComponent &child)设置子模型
Tick(float dt)每tick回调一次
SetTransform(mat4 modelTransform)设置模型转换
SetShaderProgram(gluint sp)设置着色器程序