# 文本词频与 Count-Min Sketch

这是一个用 C++ 编写的小项目，用来统计文本中的英文单词频率，并演示 Count-Min Sketch 对词频进行近似估计。

程序会将单词转成小写、移除非英文字母并做简单的后缀处理，然后计算精确词频；同时使用 xxHash 为单词映射到 sketch 的计数桶，并查询指定单词的估计频率。`main.cpp` 中以 `castle` 和 `beetle` 展示了这些操作。

运行时需要 `xxhash.h`、`wodehouse.txt` 和 `wodehouse-full.txt` 与程序位于同一目录。