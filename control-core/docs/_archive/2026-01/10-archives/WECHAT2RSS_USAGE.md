# Wechat2RSS 使用指南

## 🚨 重要说明
**你部署的是官方新版 Wechat2RSS (xlab.app版本)**，这与网上搜索到的旧版(wong2/WeChat2RSS)不同。旧版已停止维护，但你部署的是官方新版本。

## 📋 当前部署信息
- **服务地址**: http://localhost:8080
- **访问令牌**: jCIIbD3lRY
- **许可证有效期**: 2026-01-03
- **数据目录**: ./data

## 🌐 使用方法

### 1. 访问Web界面
打开浏览器访问: http://localhost:8080

### 2. 添加微信公众号订阅
根据新版Wechat2RSS的设计，通常有以下方式添加订阅：

#### 方法一：Web界面操作
1. 在浏览器中打开 http://localhost:8080
2. 查找"添加订阅"或"搜索公众号"功能
3. 输入公众号名称或ID
4. 获取RSS订阅链接

#### 方法二：API接口
使用访问令牌调用API：
```bash
# 示例API调用（具体接口需要查看文档）
curl -H "Authorization: Bearer jCIIbD3lRY" \
     "http://localhost:8080/api/v1/subscriptions"
```

#### 方法三：直接生成RSS链接
可能的格式（需要确认）：
```
http://localhost:8080/rss/{公众号ID}?token=jCIIbD3lRY
```

## 🔍 查看详细功能

### 查看API文档
访问 http://localhost:8080/docs 或查看页面底部的API文档链接

### 查看日志了解功能
```bash
docker compose logs -f wechat2rss
```

### 检查可用路由
```bash
docker compose logs wechat2rss | grep -E "(route|endpoint|api)"
```

## 📱 RSS阅读器推荐

### 在线RSS阅读器
- **Feedly**: https://feedly.com/
- **Inoreader**: https://www.inoreader.com/
- **The Old Reader**: https://theoldreader.com/

### 桌面RSS阅读器
- **QuiteRSS**: Windows/Linux桌面客户端
- **FeedDemon**: Windows经典RSS阅读器
- **Reeder**: Mac平台优秀RSS阅读器

### 移动端RSS阅读器
- **Feedly** (iOS/Android)
- **Inoreader** (iOS/Android)
- **NewsBlur** (iOS/Android)

## 🔧 管理和维护

### 查看服务状态
```bash
docker compose ps
```

### 查看订阅数据
数据存储在 `./data` 目录中，可以查看和管理已添加的订阅

### 备份数据
```bash
# 备份订阅数据
cp -r data data_backup_$(date +%Y%m%d)
```

### 更新服务
```bash
docker compose pull
docker compose up -d
```

## 📞 获取帮助

如果遇到问题：
1. 查看官方文档: https://wechat2rss.xlab.app/
2. 查看激活帮助: https://wechat2rss.xlab.app/deploy/active.html
3. 查看服务日志: `docker compose logs wechat2rss`

## 🔄 订阅链接使用示例

一旦获得了RSS订阅链接，可以在RSS阅读器中使用：

```rss
# 示例链接格式（具体格式需要确认）
http://localhost:8080/rss/tech_crunch?token=jCIIbD3lRY
http://localhost:8080/rss/wechat_mp_id?token=jCIIbD3lRY
```

## ⚠️ 注意事项

1. **令牌安全**: 不要公开分享你的访问令牌 `jCIIbD3lRY`
2. **网络访问**: 确保RSS阅读器可以访问 http://localhost:8080
3. **数据备份**: 定期备份 `./data` 目录中的订阅数据
4. **服务监控**: 定期检查服务运行状态