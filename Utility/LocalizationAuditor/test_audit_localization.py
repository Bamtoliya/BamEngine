import tempfile
import unittest
from pathlib import Path

from audit_localization import find_hardcoded_ui, load_csv_catalog, strip_comments


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


if __name__ == '__main__':
    unittest.main()
