"""Recipe Configuration Widget for Siligen HMI."""
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QLabel, QPushButton, 
    QLineEdit, QGridLayout, QFrame, QSpinBox, QDoubleSpinBox, QCheckBox, 
    QComboBox, QFormLayout, QScrollArea, QListWidget, QListWidgetItem, 
    QTabWidget, QSplitter, QMessageBox, QFileDialog
)
from PyQt5.QtCore import Qt, QSignalBlocker

from hmi_application.preview_planning_context import PreviewPlanningContextOwner

class RecipeConfigWidget(QWidget):
    """
    Widget for managing recipes, versions, and parameters.
    Refactored from MainWindow to separate concerns.
    """
    def __init__(self, protocol, planning_context_owner=None, parent=None):
        super().__init__(parent)
        self._protocol = protocol
        self._planning_context_owner = (
            planning_context_owner if planning_context_owner is not None else PreviewPlanningContextOwner()
        )
        
        # Data state
        self._recipe_templates = []
        self._recipe_schema = {}
        self._recipe_schema_entries = []
        self._recipe_versions = {}
        self._recipes = {}
        self._current_recipe = {}
        self._current_version = {}
        self._recipe_param_widgets = {}
        self._current_recipe_id = ""
        self._current_version_id = ""

        self._init_ui()
        
        # Initial load is now handled by MainWindow after connection
        # self._load_recipe_context()

    def current_recipe_selection(self):
        return self._planning_context_owner.current_selection()

    def freeze_preview_planning_context(self):
        return self._planning_context_owner.freeze_for_prepare()

    def _select_list_item_by_id(self, list_widget: QListWidget, item_id: str) -> bool:
        normalized_item_id = str(item_id or "").strip()
        if not normalized_item_id:
            return False
        for index in range(list_widget.count()):
            item = list_widget.item(index)
            if str(item.data(Qt.UserRole) or "").strip() != normalized_item_id:
                continue
            with QSignalBlocker(list_widget):
                list_widget.setCurrentItem(item)
                item.setSelected(True)
            return True
        return False

    def _apply_version_view_state(self, version_id: str) -> None:
        normalized_version_id = str(version_id or "").strip()
        version = self._recipe_versions.get(normalized_version_id, {})
        self._current_version = version
        self._current_version_id = normalized_version_id
        self._render_recipe_parameters(version.get("parameters", []), version.get("status", ""))

    def _init_ui(self):
        main_layout = QHBoxLayout(self)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(10)

        # Splitter for resizable columns
        splitter = QSplitter(Qt.Horizontal)
        
        # === Left Column: Recipe List & Basic Actions ===
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_layout.setContentsMargins(0, 0, 0, 0)
        
        # 1. Recipe Selection & List
        list_group = QGroupBox("配方列表")
        list_layout_box = QVBoxLayout(list_group)
        
        self._recipe_list_widget = QListWidget()
        self._recipe_list_widget.itemSelectionChanged.connect(self._on_recipe_selected_from_list)
        list_layout_box.addWidget(self._recipe_list_widget)
        
        btn_row = QHBoxLayout()
        self._recipe_refresh_btn = QPushButton("刷新")
        self._recipe_refresh_btn.clicked.connect(self._on_recipe_refresh)
        btn_row.addWidget(self._recipe_refresh_btn)
        
        self._recipe_create_new_btn = QPushButton("新建配方")
        self._recipe_create_new_btn.clicked.connect(self._show_create_dialog) # TODO: Implement dialog or inline
        btn_row.addWidget(self._recipe_create_new_btn)
        
        list_layout_box.addLayout(btn_row)
        left_layout.addWidget(list_group)
        
        # 2. Import/Export (Moved to bottom left)
        transfer_group = QGroupBox("导入/导出")
        transfer_layout = QGridLayout(transfer_group)
        
        self._recipe_import_resolution = QComboBox()
        self._recipe_import_resolution.addItems(["rename", "skip", "replace"])
        transfer_layout.addWidget(QLabel("冲突策略:"), 0, 0)
        transfer_layout.addWidget(self._recipe_import_resolution, 0, 1)
        
        self._recipe_import_dry_run = QCheckBox("仅校验")
        transfer_layout.addWidget(self._recipe_import_dry_run, 1, 1)
        
        transfer_btn_row = QHBoxLayout()
        self._recipe_export_btn = QPushButton("导出")
        self._recipe_export_btn.clicked.connect(self._on_recipe_export)
        transfer_btn_row.addWidget(self._recipe_export_btn)
        
        self._recipe_import_btn = QPushButton("导入")
        self._recipe_import_btn.clicked.connect(self._on_recipe_import)
        transfer_btn_row.addWidget(self._recipe_import_btn)
        
        transfer_layout.addLayout(transfer_btn_row, 2, 0, 1, 2)
        left_layout.addWidget(transfer_group)
        
        splitter.addWidget(left_widget)

        # === Right Column: Details & Editing ===
        right_widget = QWidget()
        right_layout = QVBoxLayout(right_widget)
        right_layout.setContentsMargins(0, 0, 0, 0)

        # 1. Current Recipe Header
        header_frame = QFrame()
        header_frame.setFrameShape(QFrame.StyledPanel)
        header_layout = QHBoxLayout(header_frame)
        
        self._current_recipe_title = QLabel("未选择配方")
        self._current_recipe_title.setStyleSheet("font-size: 16px; font-weight: bold;")
        header_layout.addWidget(self._current_recipe_title)
        
        self._current_recipe_status = QLabel("")
        header_layout.addWidget(self._current_recipe_status)
        
        header_layout.addStretch()
        right_layout.addWidget(header_frame)

        # 2. Tabs for Details
        self._tabs = QTabWidget()
        
        # Tab 1: Basic Info & Versioning
        self._tabs.addTab(self._create_basic_info_tab(), "基本信息 & 版本")
        
        # Tab 2: Parameters (Dynamic)
        self._tabs.addTab(self._create_params_tab(), "参数配置")
        
        # Tab 3: Audit Log
        self._tabs.addTab(self._create_audit_tab(), "审计记录")
        
        right_layout.addWidget(self._tabs)
        
        splitter.addWidget(right_widget)
        
        # Set splitter proportions (Left 1 : Right 3)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 3)
        
        main_layout.addWidget(splitter)

    def _create_basic_info_tab(self) -> QWidget:
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # Basic Info Group
        info_group = QGroupBox("基本信息")
        info_layout = QGridLayout(info_group)
        
        info_layout.addWidget(QLabel("名称:"), 0, 0)
        self._recipe_name_input = QLineEdit()
        info_layout.addWidget(self._recipe_name_input, 0, 1)
        
        info_layout.addWidget(QLabel("描述:"), 1, 0)
        self._recipe_desc_input = QLineEdit()
        info_layout.addWidget(self._recipe_desc_input, 1, 1)
        
        info_layout.addWidget(QLabel("标签:"), 2, 0)
        self._recipe_tags_input = QLineEdit()
        self._recipe_tags_input.setPlaceholderText("tag1, tag2")
        info_layout.addWidget(self._recipe_tags_input, 2, 1)
        
        update_btn = QPushButton("更新基本信息")
        update_btn.clicked.connect(self._on_recipe_update)
        info_layout.addWidget(update_btn, 3, 1, alignment=Qt.AlignRight)
        
        layout.addWidget(info_group)
        
        # Version Management Group
        ver_group = QGroupBox("版本管理")
        ver_layout = QVBoxLayout(ver_group)
        
        # Version List
        self._recipe_version_list = QListWidget()
        self._recipe_version_list.itemSelectionChanged.connect(self._on_recipe_version_selected)
        ver_layout.addWidget(self._recipe_version_list)
        
        # Version Actions
        ver_btn_row = QHBoxLayout()
        self._recipe_version_create_btn = QPushButton("新建版本/草稿")
        self._recipe_version_create_btn.clicked.connect(self._on_recipe_version_create)
        ver_btn_row.addWidget(self._recipe_version_create_btn)
        
        self._recipe_publish_btn = QPushButton("发布当前版本")
        self._recipe_publish_btn.clicked.connect(self._on_recipe_publish)
        ver_btn_row.addWidget(self._recipe_publish_btn)
        
        self._recipe_activate_btn = QPushButton("回滚/激活")
        self._recipe_activate_btn.clicked.connect(self._on_recipe_activate)
        ver_btn_row.addWidget(self._recipe_activate_btn)
        
        ver_layout.addLayout(ver_btn_row)
        
        # Draft Controls
        draft_group = QGroupBox("草稿操作")
        draft_layout = QGridLayout(draft_group)
        
        draft_layout.addWidget(QLabel("模板:"), 0, 0)
        self._recipe_template_combo = QComboBox()
        draft_layout.addWidget(self._recipe_template_combo, 0, 1)
        
        draft_layout.addWidget(QLabel("变更说明:"), 1, 0)
        self._recipe_change_note_input = QLineEdit()
        draft_layout.addWidget(self._recipe_change_note_input, 1, 1)
        
        draft_layout.addWidget(QLabel("版本标签:"), 2, 0)
        self._recipe_version_label_input = QLineEdit("v1")
        draft_layout.addWidget(self._recipe_version_label_input, 2, 1)
        
        draft_btn_row = QHBoxLayout()
        self._recipe_draft_create_btn = QPushButton("基于模板创建草稿")
        self._recipe_draft_create_btn.clicked.connect(self._on_recipe_draft_create)
        draft_btn_row.addWidget(self._recipe_draft_create_btn)
        
        self._recipe_draft_save_btn = QPushButton("保存参数到草稿")
        self._recipe_draft_save_btn.clicked.connect(self._on_recipe_draft_save)
        draft_btn_row.addWidget(self._recipe_draft_save_btn)
        
        draft_layout.addLayout(draft_btn_row, 3, 0, 1, 2)
        
        ver_layout.addWidget(draft_group)
        layout.addWidget(ver_group)
        
        return widget

    def _create_params_tab(self) -> QWidget:
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        self._recipe_params_form = QFormLayout()
        params_container = QWidget()
        params_container.setLayout(self._recipe_params_form)
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(params_container)
        
        layout.addWidget(scroll)
        
        # Save button for convenience
        save_btn = QPushButton("保存参数更改 (仅草稿)")
        save_btn.clicked.connect(self._on_recipe_draft_save)
        layout.addWidget(save_btn, alignment=Qt.AlignRight)
        
        return widget

    def _create_audit_tab(self) -> QWidget:
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        self._recipe_audit_list = QListWidget()
        layout.addWidget(self._recipe_audit_list)
        
        refresh_btn = QPushButton("刷新审计记录")
        refresh_btn.clicked.connect(self._on_recipe_audit_refresh)
        layout.addWidget(refresh_btn)
        
        return widget

    # === Logic Implementation ===

    def _show_error(self, msg: str):
        # We can use a signal or QMessageBox
        # For a widget, QMessageBox is acceptable
        QMessageBox.warning(self, "错误", msg)

    def _show_message(self, msg: str):
        # For status bar updates, we might need a signal
        # For now, simple print or pass (since we detached from MainWindow)
        # TODO: Define a signal for status updates
        print(f"[RecipeWidget] {msg}")

    def _parse_tag_text(self, text: str) -> list:
        if not text:
            return []
        return [item.strip() for item in text.split(",") if item.strip()]

    def _clear_form_layout(self, layout: QFormLayout):
        while layout.count():
            item = layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
            elif item.layout():
                while item.layout().count():
                    sub = item.layout().takeAt(0)
                    if sub.widget():
                        sub.widget().deleteLater()

    # --- Data Loading & Refresh ---

    def _on_recipe_refresh(self):
        self._load_recipe_context()

    def _load_recipe_context(self):
        ok, templates, msg = self._protocol.recipe_templates()
        if ok:
            self._recipe_templates = templates
            self._recipe_template_combo.clear()
            for tmpl in templates:
                name = tmpl.get("name", tmpl.get("id", ""))
                template_id = tmpl.get("id", "")
                self._recipe_template_combo.addItem(name, template_id)
        else:
            self._show_error(f"模板加载失败: {msg}")

        ok, schema, msg = self._protocol.recipe_schema_default()
        if ok:
            self._recipe_schema = schema
            self._recipe_schema_entries = schema.get("entries", [])
        else:
            self._show_error(f"参数模板加载失败: {msg}")

        self._refresh_recipe_list()

    def _refresh_recipe_list(self):
        ok, recipes, msg = self._protocol.recipe_list()
        if not ok:
            self._show_error(f"配方列表获取失败: {msg}")
            return

        self._recipe_list_widget.clear()
        self._recipes = {}
        for recipe in recipes:
            recipe_id = recipe.get("id", "")
            name = recipe.get("name", recipe_id)
            display = f"{name} ({recipe_id[:8]})" if recipe_id else name
            
            item = QListWidgetItem(display)
            item.setData(Qt.UserRole, recipe_id)
            self._recipe_list_widget.addItem(item)
            
            if recipe_id:
                self._recipes[recipe_id] = recipe
        self._planning_context_owner.sync_recipe_catalog(list(self._recipes.values()))
        preferred_recipe_id, _ = self.current_recipe_selection()
        if preferred_recipe_id and self._select_list_item_by_id(self._recipe_list_widget, preferred_recipe_id):
            self._load_recipe_details(preferred_recipe_id)

    def _on_recipe_selected_from_list(self):
        items = self._recipe_list_widget.selectedItems()
        if not items:
            return
        recipe_id = items[0].data(Qt.UserRole)
        self._planning_context_owner.select_recipe(recipe_id)
        self._load_recipe_details(recipe_id)

    def _load_recipe_details(self, recipe_id):
        if not recipe_id:
            return

        ok, recipe, msg = self._protocol.recipe_get(recipe_id)
        if not ok:
            self._show_error(f"加载配方失败: {msg}")
            return

        self._current_recipe = recipe
        self._current_recipe_id = recipe_id
        self._recipes[recipe_id] = recipe
        self._planning_context_owner.sync_recipe_catalog(list(self._recipes.values()))
        
        self._update_recipe_summary(recipe)
        
        # Populate Basic Info
        self._recipe_name_input.setText(recipe.get("name", ""))
        self._recipe_desc_input.setText(recipe.get("description", ""))
        tags = recipe.get("tags", [])
        if isinstance(tags, list):
            self._recipe_tags_input.setText(",".join(tags))
            
        self._refresh_versions(recipe_id)
        self._on_recipe_audit_refresh()

    def _update_recipe_summary(self, recipe: dict):
        name = recipe.get("name", "未加载")
        status = recipe.get("status", "-")
        active_version = recipe.get("activeVersionId", "-")
        
        self._current_recipe_title.setText(name)
        self._current_recipe_status.setText(f"状态: {status} | 当前版本: {active_version if active_version else '-'}")

    # --- Actions ---

    def _show_create_dialog(self):
        # For simplicity, we can use an inline check or just clear the inputs
        # But here let's just create a default one or ask user to fill inputs on the right
        # Better UX: Clear right panel inputs and set mode to "Create"
        # Since this is a refactor, we'll implement a simple Create action based on current inputs
        # or just add a simple input dialog. 
        # For now, let's reuse the logic: user types in Name/Desc in the basic info tab and clicks "Create"
        # But wait, the Basic Info tab currently has "Update".
        # Let's add a "Create New" flow. 
        # Simplified: Use a QDialog or InputDialog
        from PyQt5.QtWidgets import QInputDialog
        name, ok = QInputDialog.getText(self, "新建配方", "配方名称:")
        if ok and name:
            self._create_recipe(name)

    def _create_recipe(self, name: str):
        # Description/Tags could be optional or asked
        description = "" 
        tags = []
        
        ok, recipe, msg = self._protocol.recipe_create(
            name=name,
            description=description,
            tags=tags,
        )
        if not ok:
            self._show_error(f"创建配方失败: {msg}")
            return
        self._show_message(f"已创建配方: {recipe.get('id', '')}")
        self._refresh_recipe_list()

    def _on_recipe_update(self):
        recipe_id = self._current_recipe_id
        if not recipe_id:
            self._show_error("请先选择配方")
            return
        name = self._recipe_name_input.text().strip()
        description = self._recipe_desc_input.text().strip()
        tags = self._parse_tag_text(self._recipe_tags_input.text())
        
        ok, recipe, msg = self._protocol.recipe_update(
            recipe_id=recipe_id,
            name=name,
            description=description,
            tags=tags,
        )
        if not ok:
            self._show_error(f"更新配方失败: {msg}")
            return
        self._current_recipe = recipe
        self._update_recipe_summary(recipe)
        self._show_message("配方已更新")
        self._refresh_recipe_list()

    def _refresh_versions(self, recipe_id):
        ok, versions, msg = self._protocol.recipe_versions(recipe_id)
        if not ok:
            self._show_error(f"版本列表获取失败: {msg}")
            return
        self._recipe_versions = {}
        self._recipe_version_list.clear()
        
        for version in versions:
            version_id = version.get("id", "")
            label = version.get("versionLabel", "")
            status = version.get("status", "")
            display = f"{label} | {status} | {version_id[:8]}" if version_id else label
            item = QListWidgetItem(display)
            item.setData(Qt.UserRole, version_id)
            self._recipe_version_list.addItem(item)
            if version_id:
                self._recipe_versions[version_id] = version
        self._planning_context_owner.sync_recipe_versions(recipe_id, list(self._recipe_versions.values()))
        _, preferred_version_id = self.current_recipe_selection()
        if preferred_version_id and self._select_list_item_by_id(self._recipe_version_list, preferred_version_id):
            self._apply_version_view_state(preferred_version_id)

    def _on_recipe_version_selected(self):
        items = self._recipe_version_list.selectedItems()
        if not items:
            return
        version_id = items[0].data(Qt.UserRole)
        self._planning_context_owner.select_version(version_id)
        self._apply_version_view_state(version_id)
        
        # Switch to Params tab to show context
        # self._tabs.setCurrentIndex(1) # Optional: Auto-switch

    def _render_recipe_parameters(self, parameters: list, status: str):
        self._clear_form_layout(self._recipe_params_form)
        self._recipe_param_widgets = {}
        
        if not self._recipe_schema_entries:
            self._recipe_params_form.addRow(QLabel("未加载参数模板"))
            return
            
        param_map = {p.get("key"): p.get("value") for p in parameters if "key" in p}
        editable = status == "draft"
        
        for entry in self._recipe_schema_entries:
            key = entry.get("key", "")
            display = entry.get("displayName", key)
            unit = entry.get("unit", "")
            required = entry.get("required", False)
            if unit:
                display = f"{display} ({unit})"
            if required:
                display = f"{display} *"
                
            value_type = entry.get("type", "string")
            widget = None
            constraints = entry.get("constraints", {})
            
            if value_type == "int":
                widget = QSpinBox()
                min_value = int(constraints.get("min", -1000000))
                max_value = int(constraints.get("max", 1000000))
                widget.setRange(min_value, max_value)
                widget.setValue(int(param_map.get(key, min_value)))
            elif value_type == "float":
                widget = QDoubleSpinBox()
                min_value = float(constraints.get("min", -1000000.0))
                max_value = float(constraints.get("max", 1000000.0))
                widget.setRange(min_value, max_value)
                widget.setDecimals(3)
                widget.setValue(float(param_map.get(key, min_value)))
            elif value_type == "bool":
                widget = QCheckBox()
                widget.setChecked(bool(param_map.get(key, False)))
            elif value_type == "enum":
                widget = QComboBox()
                allowed = constraints.get("allowedValues", [])
                for option in allowed:
                    widget.addItem(option)
                current = param_map.get(key, "")
                if current and current not in allowed:
                    widget.addItem(current)
                if current:
                    widget.setCurrentText(str(current))
            else:
                widget = QLineEdit()
                widget.setText(str(param_map.get(key, "")))
                
            widget.setEnabled(editable)
            self._recipe_param_widgets[key] = {"widget": widget, "type": value_type}
            self._recipe_params_form.addRow(QLabel(display), widget)

    def _collect_recipe_parameters(self) -> list:
        parameters = []
        for key, meta in self._recipe_param_widgets.items():
            widget = meta["widget"]
            value_type = meta["type"]
            if value_type == "int":
                value = int(widget.value())
            elif value_type == "float":
                value = float(widget.value())
            elif value_type == "bool":
                value = bool(widget.isChecked())
            elif value_type == "enum":
                value = widget.currentText()
            else:
                value = widget.text()
            parameters.append({"key": key, "value": value})
        return parameters

    def _on_recipe_draft_create(self):
        if not self._current_recipe_id:
            self._show_error("请先选择配方")
            return
        template_id = self._recipe_template_combo.currentData()
        if not template_id:
            self._show_error("请选择模板")
            return
            
        version_label = self._recipe_version_label_input.text().strip()
        change_note = self._recipe_change_note_input.text().strip()
        
        ok, version, msg = self._protocol.recipe_draft_create(
            recipe_id=self._current_recipe_id,
            template_id=template_id,
            version_label=version_label,
            change_note=change_note,
        )
        if not ok:
            self._show_error(f"创建草稿失败: {msg}")
            return
            
        self._show_message(f"草稿已创建: {version.get('id', '')}")
        self._refresh_versions(self._current_recipe_id)
        # Auto select the new version logic can be improved (find by ID)

    def _on_recipe_draft_save(self):
        if not self._current_recipe_id or not self._current_version_id:
            self._show_error("请先选择草稿版本")
            return
        if self._current_version.get("status", "") != "draft":
            self._show_error("仅草稿版本可编辑")
            return
            
        parameters = self._collect_recipe_parameters()
        ok, version, msg = self._protocol.recipe_draft_update(
            recipe_id=self._current_recipe_id,
            version_id=self._current_version_id,
            parameters=parameters,
            change_note=self._recipe_change_note_input.text().strip(),
        )
        if not ok:
            self._show_error(f"保存草稿失败: {msg}")
            return
        self._show_message("草稿已更新")
        self._refresh_versions(self._current_recipe_id)

    def _on_recipe_publish(self):
        if not self._current_recipe_id or not self._current_version_id:
            self._show_error("请先选择版本")
            return
        
        ok, version, msg = self._protocol.recipe_publish(
            recipe_id=self._current_recipe_id,
            version_id=self._current_version_id,
        )
        if not ok:
            self._show_error(f"发布失败: {msg}")
            return
        self._show_message("版本已发布")
        self._refresh_versions(self._current_recipe_id)
        # Refresh summary to show new active version
        self._load_recipe_details(self._current_recipe_id)

    def _on_recipe_version_create(self):
        # Triggered by button, maybe just focus inputs or clear them
        # In this simplified flow, user just uses the "Create Draft" group
        self._tabs.setCurrentIndex(0) # Basic info tab where controls are
        self._recipe_version_label_input.setFocus()
        self._show_message("请在下方填写信息并点击'基于模板创建草稿'")

    def _on_recipe_activate(self):
        # Rollback/Activate logic
        # Not fully implemented in original code beyond basic structure
        # Assuming protocol support
        pass

    def _on_recipe_audit_refresh(self):
        if not self._current_recipe_id:
            return
        # Assuming protocol has recipe_audit(id)
        # ok, logs, msg = self._protocol.recipe_audit(self._current_recipe_id)
        # Placeholder
        self._recipe_audit_list.clear()
        self._recipe_audit_list.addItem("审计日志功能暂未连接到后端")

    def _on_recipe_export(self):
        if not self._current_recipe_id:
            self._show_error("未选择配方")
            return
        filepath, _ = QFileDialog.getSaveFileName(self, "导出配方", "", "JSON文件 (*.json)")
        if not filepath:
            return
        
        ok, msg = self._protocol.recipe_export(self._current_recipe_id, filepath)
        if ok:
            self._show_message(f"导出成功: {filepath}")
        else:
            self._show_error(f"导出失败: {msg}")

    def _on_recipe_import(self):
        filepath, _ = QFileDialog.getOpenFileName(self, "导入配方", "", "JSON文件 (*.json)")
        if not filepath:
            return
        
        resolution = self._recipe_import_resolution.currentText()
        dry_run = self._recipe_import_dry_run.isChecked()
        
        ok, result, msg = self._protocol.recipe_import(
            bundle_path=filepath,
            resolution=resolution,
            dry_run=dry_run,
        )
        if ok:
            self._show_message(f"导入成功: {result}")
            self._refresh_recipe_list()
        else:
            self._show_error(f"导入失败: {msg}")
