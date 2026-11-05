import tempfile
import unittest
from pathlib import Path

from audit_localization import (duplicate_catalog_keys, find_hardcoded_ui, find_imgui_input_reads,
    find_unregistered_actions, load_csv_catalog, nonlocal_key_positions, RE_CPP_TOKEN, strip_comments,
    editor_catalog_layout_issues, find_panel_display_name_issues)


class LocalizationAuditTests(unittest.TestCase):
    def literals(self, source):
        return [text for _, _, text in find_hardcoded_ui(source)]

    def test_widget_labels_tooltips_and_colored_text(self):
        source = '''ImGui::SliderFloat("Strength", &value, 0, 1);
            ImGui::TextColored(ImVec4(1, 1, 1, 1), "Target: %s", name);
            ImGui::SetTooltip("Use the mouse wheel.");'''
        self.assertEqual(self.literals(source), ['Strength', 'Target: %s', 'Use the mouse wheel.'])

    def test_multiline_and_conditional_labels(self):
        source = '''ImGui::Button(
            selected ? LOCAL_CSTR("Preview.Selected") : "Sphere");
            ImGui::TextUnformatted(LOCAL_CSTR("Caption")); ImGui::Button("Untranslated");'''
        self.assertEqual(self.literals(source), ['Sphere', 'Untranslated'])

    def test_comments_ids_formats_units_and_brand_are_ignored(self):
        source = '''// ImGui::Button("Comment");
            /* ImGui::SliderFloat("Comment", &v, 0, 1); */
            ImGui::InputText("##Name", buffer, size);
            ImGui::Text("%s: %llu", label, id);
            ImGui::Text("%.3f ms", value);
            ImGui::Text(ICON_FA_CUBES " BamEngine");'''
        self.assertEqual(self.literals(source), [])
        self.assertEqual(strip_comments(source).count('\n'), source.count('\n'))

    def test_visible_prefix_after_format_and_before_id_is_checked(self):
        self.assertEqual(self.literals('ImGui::Text("%s reloaded.", name);'), ['%s reloaded.'])
        self.assertEqual(self.literals('ImGui::Begin("Terrain###TerrainEditor");'), ['Terrain###TerrainEditor'])

    def test_localization_wrappers_and_fallback_helpers(self):
        source = '''ImGui::Button((ICON_FA_PLAY " " + LOCAL_LABEL("Play")).c_str());
            ImGui::MenuItem(Text("MoveUp", "Move Up").c_str());
            ImGui::Checkbox(manager.GetText("ApplyToAll").c_str(), &apply);'''
        self.assertEqual(self.literals(source), [])

    def test_notification_title_and_message(self):
        self.assertEqual(self.literals('AddNotification("Reloaded", name + " reloaded.", false);'),
            ['Reloaded', ' reloaded.'])
        self.assertEqual(self.literals('AddNotification(LOCAL("Title"), LOCAL_FMT("Reloaded", name), false);'), [])

    def test_group_inheritance_matches_converter_without_short_key_aliases(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'Editor.csv'
            path.write_text('Group,Key,English,Korean\nPanel,Title,Title,제목\n,Hint,Hint,힌트\n', encoding='utf-8')
            catalog, empty = load_csv_catalog(path)
        self.assertEqual(set(catalog), {'Panel.Title', 'Panel.Hint'})
        self.assertEqual(empty, [])

    def test_draw_text_hint_and_packed_combo_items(self):
        source = '''drawList->AddText(pos, color, "Canvas caption");
            drawList->AddText(font, size, pos, color, "Overlay caption");
            ImGui::InputTextWithHint("##Search", "Search here", buffer, size);
            ImGui::Combo("##Mode", &mode, "Repeat\\0Once\\0");'''
        self.assertEqual(self.literals(source), ['Search here', 'Repeat\\0Once\\0', 'Canvas caption', 'Overlay caption'])

    def test_tree_ids_are_not_display_text(self):
        source = '''ImGui::TreeNodeEx("StableId", flags, "%s", LOCAL_CSTR("Panel.Title"));
            ImGui::TreeNodeEx("Visible caption", flags);
            ImGui::TreeNode("StableId", "Visible format %d", count);'''
        self.assertEqual(self.literals(source), ['Visible caption', 'Visible format %d'])

    def test_hardcoded_shortcut_hints_are_checked(self):
        source = '''ImGui::MenuItem(LOCAL_LABEL("Graph.Delete").c_str(), "Del");
            ImGui::MenuItem(LOCAL_LABEL("Graph.Save").c_str(), sm.GetShortcutHint("Graph.Save"));'''
        self.assertEqual(self.literals(source), ['Del'])

    def test_action_and_workspace_identifiers_do_not_mask_locale_references(self):
        source = '''sm.GetShortcutHint("Graph.Save"); workspace.SetSplitter("Timeline.Sidebar", 10);
            ACTION_MENU_ITEM(LOCAL_LABEL("Graph.Caption"), "Graph.Save");
            LOCAL("Graph.Missing");'''
        positions = nonlocal_key_positions(source)
        keys = [token[0] for token in RE_CPP_TOKEN.finditer(source) if token.start() not in positions]
        self.assertIn('"Graph.Caption"', keys)
        self.assertIn('"Graph.Missing"', keys)
        self.assertNotIn('"Graph.Save"', keys)
        self.assertNotIn('"Timeline.Sidebar"', keys)

    def test_all_duplicate_locations_across_catalogs(self):
        with tempfile.TemporaryDirectory() as directory:
            first, second = (Path(directory) / name for name in ('Editor.csv', 'Engine.csv'))
            first.write_text('Group,Key,English,Korean\nPanel,A,A,가\n,A,A,가\n,B,B,나\n,B,B,나\n', encoding='utf-8')
            second.write_text('Group,Key,English,Korean\nPanel,A,A,가\n', encoding='utf-8')
            duplicates = duplicate_catalog_keys([first, second])
        self.assertEqual(duplicates, {'Panel.A': [('Editor.csv', 2), ('Editor.csv', 3), ('Engine.csv', 2)],
            'Panel.B': [('Editor.csv', 4), ('Editor.csv', 5)]})

    def test_editor_groups_are_explicit_without_repeated_prefixes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'Editor.csv'
            path.write_text('Group,Key,English,Korean\n,Panel.Title,Title,제목\n'
                'Panel,Title,Title,제목\nPanel,Panel.Title,Title,제목\n'
                ',Hint,Hint,힌트\nAction,Viewport.Focus,Focus,포커스\n', encoding='utf-8')
            issues = list(editor_catalog_layout_issues(path))
        self.assertEqual([line for line, _, _ in issues], [2, 4, 5])

    def test_action_identifier_is_not_a_locale_reference_during_rename(self):
        source = '''sm.RegisterAction({ .Id = "Viewport.Focus", .DisplayNameKey = "Action.Viewport.Focus" });
            sm.GetShortcutHint("Viewport.Focus");
            ACTION_MENU_ITEM(LOCAL_LABEL("Viewport.Focus"), "Viewport.Focus");'''
        identifiers = nonlocal_key_positions(source)
        visible = [token[0] for token in RE_CPP_TOKEN.finditer(source) if token.start() not in identifiers]
        self.assertEqual(visible, ['"Action.Viewport.Focus"', '"Viewport.Focus"'])

    def test_panel_name_fallback_requires_localized_display_name(self):
        sources = {'Editor/Source/AnimatorGraphPanel.h': 'class AnimatorGraphPanel : public IImGui {};',
            'Editor/Source/AnimatorGraphPanel.cpp': '''AnimatorGraphPanel::AnimatorGraphPanel() {
                m_Name = L"Animator Graph";
            } void AnimatorGraphPanel::Draw() { ImGui::Begin(GetDisplayName().c_str()); }'''}
        self.assertEqual([issue[3] for issue in find_panel_display_name_issues(sources)], ['Animator Graph'])
        sources['Editor/Source/AnimatorGraphPanel.cpp'] += 'm_DisplayNameKey = "AnimatorGraph.Title";'
        self.assertEqual(list(find_panel_display_name_issues(sources)), [])

    def test_viewport_descriptor_and_custom_names_do_not_require_fixed_caption_keys(self):
        sources = {'Editor/Source/BaseViewportPanel.h': '''class BaseViewportPanel : public IImGui {
                BaseViewportPanel() { m_Name = L"Base Viewport"; }
            };''', 'Editor/Source/BaseViewportPanel.cpp': 'm_DisplayNameKey = desc->DisplayNameKey;',
            'Editor/Source/SceneViewportPanel.h': '''class SceneViewportPanel : public BaseViewportPanel {
                SceneViewportPanel() { m_Name = L"Scene Viewport"; }
            };''', 'Editor/Source/CustomPanel.h': '''class CustomPanel : public IImGui {
                CustomPanel() { m_Name = L"Custom"; }
                string GetDisplayName() const override { return document.Name; }
            };'''}
        self.assertEqual(list(find_panel_display_name_issues(sources)), [])

    def test_missing_action_in_menu_helper_and_direct_calls(self):
        sources = {'Registry.cpp': 'sm.RegisterAction({ .Id = "Graph.Save" });',
            'Panel.cpp': '''sm.GetShortcutHint("Graph.Save");
                sm.GetShortcutHint("Graph.Unknown");
                ACTION_MENU_ITEM(LOCAL_LABEL("Save"), "Graph.Missing");
                // sm.ExecuteAction("Commented.Action");'''}
        self.assertEqual([issue[3] for issue in find_unregistered_actions(sources)], ['Graph.Unknown', 'Graph.Missing'])

    def test_raw_input_queries_are_distinct_from_ui_context_and_backend_writes(self):
        source = '''ImGui::IsWindowHovered(); ImGui::IsItemActive();
            ImGui::IsKeyPressed(key); auto mouse = io.MousePos;
            ImGui::GetIO().MousePos = hiddenPosition;
            auto wheel = ImGui::GetIO().MouseWheel; // ImGui::IsMouseDown(0);'''
        self.assertEqual([call for _, call in find_imgui_input_reads(source)],
            ['ImGui::IsKeyPressed', 'io.MousePos', 'ImGui::GetIO().MouseWheel'])


if __name__ == '__main__':
    unittest.main()
