# 贡献指南 / Contributing

感谢你愿意为极验云 SDK 出力。

## 可以提什么

- 算法实现与标准向量不一致（附 RFC/NIST 出处）
- 编译错误、平台兼容问题（附编译器版本与完整日志）
- 文档错误、示例跑不通
- 内存安全与性能问题

## 不可以提什么

- 服务端、卡密系统、管理后台相关（走官方工单）
- 注入器、内存修改、绕过检测
- 任何真实密钥、账号、用户数据、未公开的私域地址

## 流程

1. Fork → 新建分支 `fix/xxx` 或 `feat/xxx`
2. 改动尽量小，一个 PR 只做一件事
3. 必须跑过 `g++ -std=c++17 -O2 -Iinclude/jiyan tests/crypto_vectors.cpp -o cv && ./cv`
4. 涉及算法的改动，附标准测试向量与来源
5. PR 描述写清楚：改了什么 / 为什么 / 怎么验证的

## 代码风格

- C++17，不引入新依赖（现有仅 nlohmann/json）
- 对外符号沿用 `JiYan_` 前缀；新增内部函数用 `static` 或匿名命名空间
- 错误路径返回空字符串/空 vector，不抛异常
- 注释用中文，公开文档双语

## 许可证

提交即表示你同意以 MIT 许可证授权你的贡献。

---

# Contributing (English)

Fork, branch, keep PRs small, and run the crypto vector test before submitting.
No new dependencies. Do not include real keys, accounts, or user data.
By submitting a PR you agree to license your contribution under MIT.
