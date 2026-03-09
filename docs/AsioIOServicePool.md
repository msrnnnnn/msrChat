# AsioIOServicePool 文档

## 动态调整线程池大小

### 描述

`AsioIOServicePool` 类提供了一个新的接口或方法来动态调整线程池的大小。

### 使用方法

要动态调整线程池的大小，请使用 `resizeThreadPool` 方法。

#### 示例代码

```cpp
#include "AsioIOServicePool.h"

int main() {
    AsioIOServicePool pool(4);
    // 调整线程池大小到8
    pool.resizeThreadPool(8);
    return 0;
}
```

### 参数说明

- `size`: 新的线程池大小，必须是一个正整数。

### 注意事项

- 调整线程池大小可能会影响性能，请谨慎使用。
