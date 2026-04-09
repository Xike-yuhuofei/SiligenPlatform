cmake_minimum_required(VERSION 3.20)

# tests/unit 不再承载 workflow 源码级测试。
# workflow 的 source-bearing tests 仅允许存在于 tests/canonical；
# integration / regression 目录负责各自 smoke / cross-owner 证明。
