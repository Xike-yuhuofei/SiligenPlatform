# workflow/domain/domain 已从 live build graph 退化为 dormant physical residue。
# round 23 起,最后一组 dead-shell 目标已删除。
# workflow/domain requires canonical siligen_motion target and no longer defines any fallback owner target here.
# 本目录不再定义任何 live target,仅保留物理残留供后续 residue audit 使用。

message(STATUS "[Domain Root] bridge-domain root is dormant and defines no live targets")
