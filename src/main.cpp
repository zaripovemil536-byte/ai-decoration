#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include <cmath>
#include <random>

using namespace geode::prelude;

class $modify(DecoEditorUI, EditorUI) {
	struct Fields {
		std::mt19937 rng;
		int placed = 0;
		float originX = 0.f;
	};

	// ---------- helpers ----------
	float rnd(float a, float b) {
		return std::uniform_real_distribution<float>(a, b)(m_fields->rng);
	}
	int rndi(int a, int b) {
		return std::uniform_int_distribution<int>(a, b)(m_fields->rng);
	}
	GameObject* put(int id, float x, float y, float scale, float rot = 0.f) {
		auto obj = m_editorLayer->createObject(id, {m_fields->originX + x, y}, false);
		if (!obj) return nullptr;
		obj->setScale(scale);
		if (rot != 0.f) obj->setRotation(rot);
		m_fields->placed++;
		return obj;
	}
	// filled circle made of small blocks
	void disc(float cx, float cy, float r, int id, float step = 15.f, float scale = 0.5f) {
		for (float dx = -r; dx <= r; dx += step) {
			for (float dy = -r; dy <= r; dy += step) {
				if (dx * dx + dy * dy <= r * r) put(id, cx + dx, cy + dy, scale);
			}
		}
	}

	// ---------- themes ----------
	void genSpace(int n) {
		for (int i = 0; i < n * 6; i++) {
			put(1, rnd(0, 2400), rnd(120, 560), rnd(0.12f, 0.3f));
		}
		int planets = std::clamp(n / 6, 1, 6);
		for (int i = 0; i < planets; i++) {
			float cx = rnd(150, 2250);
			float cy = rnd(240, 470);
			float r = rnd(40, 85);
			disc(cx, cy, r, 1);
			// ring made of tiny blocks
			for (float a = 0; a < 6.283f; a += 0.18f) {
				put(2, cx + std::cos(a) * r * 1.6f, cy + std::sin(a) * r * 0.5f, 0.25f);
			}
		}
	}

	void genForest(int n) {
		for (int i = 0; i < n; i++) {
			float x = rnd(0, 2400);
			int trunk = rndi(4, 10);
			for (int k = 0; k < trunk; k++) {
				put(1, x, 7.5f + k * 15.f, 0.5f);
			}
			float r = rnd(35, 70);
			float top = trunk * 15.f;
			disc(x, top + r * 0.6f, r, 1);
		}
	}

	void genCity(int n) {
		float x = 0.f;
		for (int i = 0; i < n; i++) {
			int w = rndi(1, 3);
			int h = rndi(2, 9);
			for (int bx = 0; bx < w; bx++) {
				for (int k = 0; k < h; k++) {
					put(1, x + bx * 60.f + 30.f, 30.f + k * 60.f, 2.f);
				}
			}
			// antenna on some buildings
			if (rndi(0, 2) == 0) {
				for (int k = 0; k < 3; k++) {
					put(1, x + w * 30.f, h * 60.f + 8.f + k * 15.f, 0.5f);
				}
			}
			x += w * 60.f + rnd(0, 60);
		}
	}

	void genNeon(int n) {
		for (int i = 0; i < n; i++) {
			float cx = rnd(60, 2340);
			float cy = rnd(100, 520);
			float base = rnd(0.8f, 1.6f);
			for (int k = 0; k < 4; k++) {
				put(1, cx, cy, base - k * 0.2f, 45.f);
			}
		}
	}

	void genWaves(int n) {
		int waves = std::clamp(n / 4, 3, 12);
		for (int w = 0; w < waves; w++) {
			float baseY = 90.f + w * 45.f;
			float amp = rnd(15, 55);
			float freq = rnd(0.004f, 0.012f);
			float ph = rnd(0, 6.283f);
			float s = rnd(0.3f, 0.5f);
			for (float x = 0; x < 2400; x += 24) {
				put(1, x, baseY + amp * std::sin(x * freq + ph), s);
			}
		}
	}

	// ---------- UI ----------
	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;

		auto spr = ButtonSprite::create("AI", 30, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(DecoEditorUI::onDecorate));
		auto menu = CCMenu::create();
		menu->addChild(btn);
		auto win = CCDirector::get()->getWinSize();
		menu->setPosition({win.width - 40.f, win.height - 40.f});
		menu->setZOrder(100);
		this->addChild(menu);
		return true;
	}

	void onDecorate(CCObject*) {
		auto theme = Mod::get()->getSettingValue<std::string>("theme");
		int n = static_cast<int>(Mod::get()->getSettingValue<int64_t>("count"));
		auto seed = Mod::get()->getSettingValue<int64_t>("seed");

		if (seed != 0) m_fields->rng.seed(static_cast<uint32_t>(seed));
		else m_fields->rng.seed(std::random_device{}());

		m_fields->placed = 0;
		m_fields->originX = -m_editorLayer->m_objectLayer->getPositionX() + 60.f;

		if (theme == "space") genSpace(n);
		else if (theme == "forest") genForest(n);
		else if (theme == "city") genCity(n);
		else if (theme == "neon") genNeon(n);
		else if (theme == "waves") genWaves(n);
		else {
			Notification::create("Unknown theme", NotificationIcon::Error)->show();
			return;
		}

		Notification::create(
			fmt::format("Objects added: {}", m_fields->placed),
			NotificationIcon::Success
		)->show();
	}
};
