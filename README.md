## jcyFileSystem
# c语言实现的文件系统
## 结构
# 逻辑结构：流式存储（都存为char）
# 物理结构：单级索引 
# 目录结构：混合索引，i节点 
## 功能
# open:把物理磁盘中文件信息添加到内存中，难点在多层cd和.与..的特殊情况(cd 实际上也是调用的这个，只是需要多考虑..情况)

