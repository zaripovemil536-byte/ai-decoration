#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

using namespace geode::prelude;

// ID объектов, которые разрешаем ИИ использовать (можно расширить).
static const char* ALLOWED_IDS = "1,2,3,4,5,6,7,40";

class $modify(AIEditorUI, EditorUI) {
	struct Fields {
		async::TaskHolder<web::WebResponse> m_holder;
		bool m_busy = false;
	};

	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;

		auto spr = ButtonSprite::create("AI", 30, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AIEditorUI::onAIDecorate));
		auto menu = CCMenu::create();
		menu->addChild(btn);
		auto win = CCDirector::get()->getWinSize();
		menu->setPosition({win.width - 40.f, win.height - 40.f});
		menu->setZOrder(100);
		this->addChild(menu);
		return true;
	}

	void onAIDecorate(CCObject*) {
		if (m_fields->m_busy) return;

		auto key = Mod::get()->getSettingValue<std::string>("api-key");
		if (key.empty()) {
			FLAlertLayer::create("AI Decorator", "Укажи API ключ в настройках мода.", "OK")->show();
			return;
		}
		auto theme = Mod::get()->getSettingValue<std::string>("theme");
		auto model = Mod::get()->getSettingValue<std::string>("model");
		auto count = Mod::get()->getSettingValue<int64_t>("count");

		std::string prompt = fmt::format(
			"Ты помощник по декору уровней Geometry Dash. Тема: \"{}\". "
			"Расставь примерно {} декоративных объектов в области x от 0 до 1500 и y от 60 до 400. "
			"Используй ТОЛЬКО эти id объектов: {}. "
			"Сделай композицию осмысленной и красивой (группы, симметрия, разный масштаб). "
			"Ответь ТОЛЬКО JSON-массивом без пояснений и без markdown, формат: "
			"[{{\"id\":1,\"x\":100,\"y\":120,\"scale\":1.0,\"rot\":0}}]",
			theme, count, ALLOWED_IDS
		);

		matjson::Value msg = matjson::Value::object();
		msg["role"] = "user";
		msg["content"] = prompt;
		matjson::Value msgs = matjson::Value::array();
		msgs.push(msg);
		matjson::Value body = matjson::Value::object();
		body["model"] = model;
		body["max_tokens"] = 4000;
		body["messages"] = msgs;

		m_fields->m_busy = true;
		Notification::create("ИИ думает...", NotificationIcon::Loading)->show();

		auto req = web::WebRequest();
		req.header("x-api-key", key);
		req.header("anthropic-version", "2023-06-01");
		req.header("content-type", "application/json");
		req.bodyJSON(body);

		Ref<AIEditorUI> self = this;
		m_fields->m_holder.spawn(
			req.post("https://api.anthropic.com/v1/messages"),
			[self](web::WebResponse res) {
				self->m_fields->m_busy = false;
				if (!res.ok()) {
					Notification::create(
						fmt::format("Ошибка API: {}", res.code()),
						NotificationIcon::Error
					)->show();
					return;
				}
				self->applyResponse(res.json().unwrapOr(matjson::Value()));
			}
		);
	}

	void applyResponse(matjson::Value const& json) {
		// Достаём текст ответа: content[0].text
		std::string text;
		if (auto content = json["content"].asArray()) {
			if (!content.unwrap().empty()) {
				text = content.unwrap()[0]["text"].asString().unwrapOr("");
			}
		}
		// Вырезаем JSON-массив, даже если модель добавила лишнее
		auto a = text.find('[');
		auto b = text.rfind(']');
		if (a == std::string::npos || b == std::string::npos || b < a) {
			Notification::create("ИИ вернул странный ответ", NotificationIcon::Error)->show();
			return;
		}
		auto parsed = matjson::parse(text.substr(a, b - a + 1));
		if (!parsed || !parsed.unwrap().isArray()) {
			Notification::create("Не смог разобрать JSON", NotificationIcon::Error)->show();
			return;
		}

		// Начало координат — левый край текущей камеры редактора
		float originX = -m_editorLayer->m_objectLayer->getPositionX();
		int placed = 0;

		for (auto const& o : parsed.unwrap().asArray().unwrap()) {
			int id = static_cast<int>(o["id"].asInt().unwrapOr(0));
			float x = static_cast<float>(o["x"].asDouble().unwrapOr(0));
			float y = static_cast<float>(o["y"].asDouble().unwrapOr(0));
			float scale = static_cast<float>(o["scale"].asDouble().unwrapOr(1));
			float rot = static_cast<float>(o["rot"].asDouble().unwrapOr(0));
			if (id <= 0) continue;

			auto obj = m_editorLayer->createObject(id, {originX + x, y}, false);
			if (!obj) continue;
			obj->setScale(std::clamp(scale, 0.2f, 4.f));
			obj->setRotation(rot);
			placed++;
		}

		Notification::create(
			fmt::format("Добавлено объектов: {}", placed),
			NotificationIcon::Success
		)->show();
	}
};
