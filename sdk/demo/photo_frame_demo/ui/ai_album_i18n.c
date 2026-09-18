#include "ui/ai_album_i18n.h"

#include "ui/ai_album_font_manager.h"

#include <string.h>

typedef struct {
    const char *english;
    const char *chinese;
    const char *japanese;
} ai_album_translation_t;

static const ai_album_translation_t g_translations[] = {
    {"A warm companion for everyday conversation", "温暖陪伴你的日常交流伙伴", "日常会話に寄り添う温かなパートナー"},
    {"A practical guide for your next journey", "为下一次旅程提供实用建议", "次の旅を支える実用的なガイド"},
    {"An imaginative companion for family stories", "富有想象力的家庭故事伙伴", "家族の物語を彩る想像力豊かな仲間"},
    {"A patient guide for study plans and explanations", "耐心讲解并协助制定学习计划", "学習計画と解説を支える丁寧なガイド"},
    {"A friendly helper for everyday home cooking", "帮助完成日常家庭烹饪", "毎日の家庭料理を助ける親切な相棒"},
    {"A gentle coach for routines, movement and mood", "温和陪伴作息、运动和情绪管理", "生活習慣・運動・気分を支える優しいコーチ"},
    {"A1 / A2  SPEAKING", "A1 / A2  口语", "A1 / A2  スピーキング"},
    {"0 PHOTOS", "0 张照片", "写真 0 枚"},
    {"AI: ", "AI：", "AI："},
    {"YOU: ", "你：", "あなた："},
    {"ARROWS MOVE   OK TYPE   SELECT SEARCH WHEN READY", "方向键移动   OK 输入   准备好后选择搜索", "矢印 移動   OK 入力   準備後に検索"},
    {"MODEL        AI ALBUM TXW827\n\nFIRMWARE     UI PROTOTYPE 0.2\n\nSCREEN       1024 x 600 RGB\n\nDEVICE ID    FRAME-DEMO-001\n\nSERVICES     OFFLINE PLACEHOLDERS", "型号        AI ALBUM TXW827\n\n固件        UI 原型 0.2\n\n屏幕        1024 x 600 RGB\n\n设备 ID     FRAME-DEMO-001\n\n服务        离线占位", "モデル      AI ALBUM TXW827\n\nファーム    UIプロトタイプ 0.2\n\n画面        1024 x 600 RGB\n\n端末 ID     FRAME-DEMO-001\n\nサービス    オフライン仮表示"},
    {"%u PHOTOS", "%u 张照片", "%u 枚の写真"},
    {"DAILY GREETING", "日常问候", "日常のあいさつ"},
    {"DIRECTIONS", "问路交流", "道案内"},
    {"OK  REPLAY", "OK  重播", "OK  再生"},
    {"OK  START", "OK  开始", "OK  開始"},
    {"RESTAURANT", "餐厅点餐", "レストラン"},
    {"SHOPPING", "购物交流", "買い物"},
    {"TRANSPORT", "交通出行", "交通"},
    {"TRAVEL", "旅行入住", "旅行"},
    {"VOICE SERVICE OFFLINE", "语音服务未连接", "音声サービスはオフラインです"},
    {"CONNECTING TO VOICE SERVICE...", "正在连接语音服务...", "音声サービスに接続中..."},
    {"VOICE READY - HOLD M TO SPEAK", "准备好了，请长按 M 键", "準備完了 - Mを長押しして話す"},
    {"LISTENING - RELEASE M TO SEND", "正在聆听，松开 M 键发送", "聞き取り中 - Mを離して送信"},
    {"RECOGNIZING VOICE...", "正在识别语音...", "音声を認識中..."},
    {"PARTNER IS THINKING...", "对话伙伴正在思考...", "会話相手が考えています..."},
    {"PARTNER IS SPEAKING...", "对话伙伴正在说话...", "会話相手が話しています..."},
    {"VOICE SERVICE UNAVAILABLE", "语音服务异常", "音声サービスを利用できません"},
    {"TURN: %u", "当前轮次：%u", "現在のターン：%u"},
    {"SELECTING SCENE: L/R SCENE, M LANGUAGE", "正在选择场景：左右切换，M 键选择语言", "シーン選択中：左右で切替、Mで言語選択"},
    {"SELECTING LANGUAGE: L/R LANGUAGE, BACK TO SCENE", "正在选择语言：左右切换，返回键回到场景", "言語選択中：左右で切替、戻るでシーンへ"},
    {"PREPARING %s VOICE... SELECT OR OK TO ENTER", "正在准备 %s 语音服务... 可继续选择，OK 进入", "%s音声を準備中... 選択を続けるかOKで入る"},
    {"%s VOICE LANGUAGE READY - OK TO START", "%s 语音语言已设置，按 OK 开始", "%s音声の準備完了 - OKで開始"},
    {"VOICE TEMPORARILY UNAVAILABLE - RETRY IN SESSION", "语音服务暂不可用，进入后将自动重试", "音声サービスを一時利用できません。開始後に再試行します"},
    {"VOICE START FAILED - CHECK NETWORK", "语音服务启动失败，请检查网络后重试", "音声サービスを開始できません。ネットワークを確認してください"},
    {"ABOUT", "关于", "端末情報"},
    {"ABOUT AI FRAME", "关于 AI 相框", "AIフレームについて"},
    {"ACTION: %s", "操作：%s", "操作：%s"},
    {"AI CHAT", "AI 对话", "AIチャット"},
    {"AI FRAME", "AI 相框", "AIフレーム"},
    {"AI GENERATED", "AI 生成", "AI生成"},
    {"AI SERVICE UNAVAILABLE", "AI 服务不可用", "AIサービスを利用できません"},
    {"AI SPEAKING", "AI 口语练习", "AIスピーキング"},
    {"ALBUM", "相册", "アルバム"},
    {"ALBUM / AI CREATE", "相册 / AI 创作", "アルバム / AI生成"},
    {"ANIME", "动漫", "アニメ"},
    {"Arabic", "阿拉伯语", "アラビア語"},
    {"ARROWS MOVE   OK TYPE   DOWN ACTIONS", "方向键移动   OK 输入   下键选择操作", "矢印 移動   OK 入力   下 操作"},
    {"ARROWS SELECT   OK APPLY   POWER EDIT QUERY", "方向键选择   OK 应用   电源键编辑", "矢印 選択   OK 適用   電源 検索編集"},
    {"ARROWS SELECT   OK CONFIRM   POWER CANCEL", "方向键选择   OK 确认   电源键取消", "矢印 選択   OK 確認   電源 キャンセル"},
    {"AVAILABLE", "可用", "利用可能"},
    {"BAIDU  >  CONNECTING", "百度服务  >  连接中", "Baidu  >  接続中"},
    {"BAIDU  >  ERROR", "百度服务  >  错误", "Baidu  >  エラー"},
    {"BAIDU  >  OFFLINE", "百度服务  >  离线", "Baidu  >  オフライン"},
    {"Baidu translation appears here", "百度翻译结果将显示在这里", "Baidu翻訳結果がここに表示されます"},
    {"BLUETOOTH", "蓝牙", "Bluetooth"},
    {"BOTH", "两者都清除", "両方"},
    {"BRIGHTNESS", "亮度", "明るさ"},
    {"BRIGHTNESS PREVIEW FAILED", "亮度预览失败", "明るさのプレビューに失敗しました"},
    {"BRIGHTNESS SAVE FAILED", "亮度保存失败", "明るさの保存に失敗しました"},
    {"CANCEL", "取消", "キャンセル"},
    {"CHARGING", "充电中", "充電中"},
    {"CHANGES ARE PREVIEWED BEFORE SAVING", "保存前会实时预览亮度", "保存前に明るさをプレビューします"},
    {"CHINESE", "中文", "中国語"},
    {"CHOOSE A PHOTO", "选择一张照片", "写真を選択"},
    {"Choose an action for the selected photo.", "选择对当前照片执行的操作。", "選択した写真の操作を選んでください。"},
    {"CITY ACTIVE - PERSISTENCE FAILED", "城市已生效，但保存失败", "都市は適用されましたが保存に失敗しました"},
    {"CITY NAME IS TOO LONG", "城市名称过长", "都市名が長すぎます"},
    {"CITY RESULTS", "城市搜索结果", "都市検索結果"},
    {"CITY SEARCH FAILED - POWER BACK TO RETRY", "城市搜索失败，按电源键返回重试", "都市検索に失敗しました。電源で戻って再試行"},
    {"CLEAR DATA, KEEP FONTS", "清除数据，保留字模", "データを消去、フォントは保持"},
    {"CLEAR SKY", "晴朗", "快晴"},
    {"CLOUDY", "多云", "曇り"},
    {"CONFIRM STORAGE RESET", "确认清除存储", "ストレージ消去の確認"},
    {"CONNECT", "连接", "接続"},
    {"CONNECTED: %s (%s)", "已连接：%s（%s）", "接続済み：%s（%s）"},
    {"CONNECTING: %s ...", "正在连接：%s ...", "接続中：%s ..."},
    {"CONNECT FAILED: %d", "连接失败：%d", "接続失敗：%d"},
    {"CONNECTING  ·  SAVE FAILED", "正在连接  ·  保存失败", "接続中  ·  保存失敗"},
    {"%s %s RSSI:%d", "%s %s 信号:%d", "%s %s RSSI:%d"},
    {"%s   FEELS %s   HUMIDITY %d%%", "%s   体感 %s   湿度 %d%%", "%s   体感 %s   湿度 %d%%"},
    {"CONNECTED - OBTAINING IP...", "已连接，正在获取 IP...", "接続済み、IP取得中..."},
    {"CONNECT FAILED - TRY AGAIN", "连接失败，请重试", "接続に失敗しました。再試行してください"},
    {"CONNECTING", "连接中", "接続中"},
    {"CONNECTING...", "连接中...", "接続中..."},
    {"CONVERSATION", "对话", "会話"},
    {"CURRENT", "当前", "現在"},
    {"CURRENT: %s", "当前：%s", "現在：%s"},
    {"CURRENT: FIXED DISPLAY", "当前：固定显示", "現在：固定表示"},
    {"CURRENT: SLIDESHOW ON", "当前：幻灯片已开启", "現在：スライドショー ON"},
    {"Danish", "丹麦语", "デンマーク語"},
    {"DELETE FAILED", "删除失败", "削除に失敗しました"},
    {"DELETE PHOTO", "删除照片", "写真を削除"},
    {"DEVICE PREFERENCES", "设备偏好设置", "端末設定"},
    {"DISPLAY LANGUAGE", "显示语言", "表示言語"},
    {"Dutch", "荷兰语", "オランダ語"},
    {"ENGLISH", "英语", "英語"},
    {"ENTER AN ENGLISH CITY NAME", "输入英文城市名称", "英語の都市名を入力"},
    {"ENTER AT LEAST TWO CHARACTERS", "请至少输入两个字符", "2文字以上入力してください"},
    {"ENTER PASSWORD", "输入密码", "パスワードを入力"},
    {"ERROR", "错误", "エラー"},
    {"FAMILY CHEF", "家庭厨师", "ファミリーシェフ"},
    {"Filipino", "菲律宾语", "フィリピン語"},
    {"Finnish", "芬兰语", "フィンランド語"},
    {"FLASH", "Flash", "Flash"},
    {"FOG", "雾", "霧"},
    {"FORMAT FLASH", "格式化 Flash", "Flashをフォーマット"},
    {"FOUND %u NETWORKS", "发现 %u 个网络", "%u 件のネットワーク"},
    {"FRAME SETTINGS", "相框设置", "フレーム設定"},
    {"French", "法语", "フランス語"},
    {"GALLERY", "图库", "ギャラリー"},
    {"GENERATE FIRST", "请先生成图片", "先に画像を生成してください"},
    {"GENERATED PREVIEW", "AI 生成预览", "AI生成プレビュー"},
    {"GENERATING...", "正在生成...", "生成中..."},
    {"GENERATION FAILED", "生成失败", "生成に失敗しました"},
    {"GENERATION START FAILED", "启动生成失败", "生成を開始できませんでした"},
    {"German", "德语", "ドイツ語"},
    {"GO BACK", "返回", "戻る"},
    {"GOOD AFTERNOON", "下午好", "こんにちは"},
    {"GOOD EVENING", "晚上好", "こんばんは"},
    {"GOOD MORNING", "早上好", "おはようございます"},
    {"Greek", "希腊语", "ギリシャ語"},
    {"Hebrew", "希伯来语", "ヘブライ語"},
    {"Hindi", "印地语", "ヒンディー語"},
    {"HOLD M TO SPEAK", "长按 M 键说话", "Mを長押しして話す"},
    {"HOLD M TO SPEAK  RELEASE TO SEND", "长按 M 键说话，松开发送", "Mを長押しして話し、離して送信"},
    {"HOLD M TO TALK", "长按 M 键对话", "Mを長押しして会話"},
    {"Hold M to speak", "长按 M 键说话", "Mを長押しして話す"},
    {"IMAGE AI", "AI 图片", "画像AI"},
    {"IMAGE FILE UNAVAILABLE", "图片文件不可用", "画像ファイルを利用できません"},
    {"IMAGE TO IMAGE", "以图生图", "画像から画像"},
    {"ABSTRACT EDITORIAL", "抽象编辑", "アブストラクト編集"},
    {"ALBUM FULL", "相册已满", "アルバムがいっぱいです"},
    {"GATHERED ZINE", "聚景志", "ギャザードジン"},
    {"GENERATION TIMEOUT", "生成超时", "生成がタイムアウトしました"},
    {"HANDCRAFTED", "手工涂鸦", "ハンドクラフト"},
    {"MINIMAL ZINE", "极简志", "ミニマルジン"},
    {"POSTCARD", "明信片", "ポストカード"},
    {"STORAGE WRITE FAILED", "存储写入失败", "ストレージ書き込みに失敗しました"},
    {"Indonesian", "印度尼西亚语", "インドネシア語"},
    {"Italian", "意大利语", "イタリア語"},
    {"Japanese", "日语", "日本語"},
    {"JAPANESE", "日语", "日本語"},
    {"JPEG FORMAT NOT SUPPORTED", "不支持此 JPEG 格式", "このJPEG形式には対応していません"},
    {"KEEP HOLDING M", "请继续按住 M 键", "Mを押し続けてください"},
    {"KEEP SD FONTS", "保留 SD 卡字模", "SDフォントを保持"},
    {"KEEP SYSTEM FONTS", "保留系统字模", "システムフォントを保持"},
    {"Korean", "韩语", "韓国語"},
    {"LANGUAGE", "语言", "言語"},
    {"LANGUAGE SAVE FAILED", "语言保存失败", "言語の保存に失敗しました"},
    {"LEFT / RIGHT PREVIEW   OK SAVE   POWER CANCEL", "左右键预览   OK 保存   电源键取消", "左右 プレビュー   OK 保存   電源 キャンセル"},
    {"LEFT / RIGHT SELECT   OK APPLY   POWER CANCEL", "左右键选择   OK 应用   电源键取消", "左右 選択   OK 適用   電源 キャンセル"},
    {"LEFT / RIGHT SELECT   OK CONFIRM   POWER CANCEL", "左右键选择   OK 确认   电源键取消", "左右 選択   OK 確認   電源 キャンセル"},
    {"LEFT / RIGHT SELECT   OK CONTINUE   POWER CANCEL", "左右键选择   OK 继续   电源键取消", "左右 選択   OK 次へ   電源 キャンセル"},
    {"LIFE PARTNER", "生活伙伴", "ライフパートナー"},
    {"LISTENING  >  RELEASE M TO FINISH", "正在聆听  >  松开 M 键结束", "聞き取り中  >  Mを離して終了"},
    {"LISTENING  >  RELEASE M TO SEND", "正在聆听  >  松开 M 键发送", "聞き取り中  >  Mを離して送信"},
    {"LISTENING - OK TO SEND", "正在聆听，按 OK 发送", "聞き取り中 - OKで送信"},
    {"Listening...\nSpeak now", "正在聆听...\n请开始说话", "聞き取り中...\n話してください"},
    {"LIVE TRANSLATE", "实时翻译", "リアルタイム翻訳"},
    {"LOADING PHOTO", "正在载入照片", "写真を読み込み中"},
    {"LOCATION", "位置", "地域"},
    {"LOCATION APPLIED", "位置已应用", "地域を適用しました"},
    {"LOCATION SAVE FAILED", "位置保存失败", "地域の保存に失敗しました"},
    {"MAINLY CLEAR", "大致晴朗", "ほぼ晴れ"},
    {"Malay", "马来语", "マレー語"},
    {"MEDIUM", "中等", "普通"},
    {"MODEL AND VERSION", "型号和版本", "モデルとバージョン"},
    {"NEED TWO PHOTOS FOR SLIDESHOW", "幻灯片至少需要两张照片", "スライドショーには写真が2枚必要です"},
    {"NETWORK AND CONNECTION", "网络和连接", "ネットワークと接続"},
    {"NETWORK UNAVAILABLE - POWER BACK AND RETRY", "网络不可用，按电源键返回重试", "ネットワークを利用できません。電源で戻って再試行"},
    {"NO", "否", "いいえ"},
    {"NO PHOTO", "无照片", "写真なし"},
    {"NO PHOTOS", "没有照片", "写真がありません"},
    {"NO MATCHING CITY - POWER BACK TO EDIT", "未找到城市，按电源键返回编辑", "該当する都市がありません。電源で戻って編集"},
    {"Norwegian", "挪威语", "ノルウェー語"},
    {"NOT AVAILABLE", "不可用", "利用不可"},
    {"NOW", "现在", "現在"},
    {"OFF", "关", "オフ"},
    {"OFFLINE", "离线", "オフライン"},
    {"OIL PAINT", "油画", "油絵"},
    {"OK OR POWER TO CLOSE", "按 OK 或电源键关闭", "OKまたは電源で閉じる"},
    {"ON", "开", "オン"},
    {"ONLINE", "在线", "オンライン"},
    {"OPEN", "开放", "オープン"},
    {"OPEN GENERATION", "打开生成", "生成を開く"},
    {"OVERCAST", "阴天", "曇天"},
    {"PAGE %u / %u", "第 %u / %u 页", "ページ %u / %u"},
    {"PARTLY CLOUDY", "局部多云", "一部曇り"},
    {"PASSWORD IS TOO LONG", "密码过长", "パスワードが長すぎます"},
    {"PASSWORD MUST BE >= 8 CHARACTERS", "密码至少需要 8 个字符", "パスワードは8文字以上必要です"},
    {"PHOTO ACTION", "照片操作", "写真の操作"},
    {"Polish", "波兰语", "ポーランド語"},
    {"Portuguese", "葡萄牙语", "ポルトガル語"},
    {"PREVIEW ONLY", "仅预览", "プレビューのみ"},
    {"PRESS M TO OPEN MENU", "按 M 键打开菜单", "Mでメニューを開く"},
    {"POWER BACK   ARROWS SELECT   OK ACTION", "电源键返回   方向键选择   OK 操作", "電源 戻る   矢印 選択   OK 操作"},
    {"POWER BACK   ARROWS NAV   OK GENERATE / SAVE", "电源键返回   方向键切换   OK 生成 / 保存", "電源 戻る   矢印 切替   OK 生成 / 保存"},
    {"POWER BACK   ARROWS SELECT   OK APPLY / SEARCH", "电源键返回   方向键选择   OK 应用 / 搜索", "電源 戻る   矢印 選択   OK 適用 / 検索"},
    {"POWER BACK   ARROWS SELECT   OK CONFIRM", "电源键返回   方向键选择   OK 确认", "電源 戻る   矢印 選択   OK 確認"},
    {"POWER BACK   ARROWS SELECT   OK CONNECT", "电源键返回   方向键选择   OK 连接", "電源 戻る   矢印 選択   OK 接続"},
    {"POWER BACK   ARROWS SELECT   OK CONNECT   M RESCAN", "电源键返回   方向键选择   OK 连接   M 重新扫描", "電源 戻る   矢印 選択   OK 接続   M 再スキャン"},
    {"POWER BACK   ARROWS SELECT   OK OPEN / TOGGLE", "电源键返回   方向键选择   OK 打开 / 切换", "電源 戻る   矢印 選択   OK 開く / 切替"},
    {"POWER BACK   ARROWS SELECT   OK VIEW   HOLD OK ACTIONS", "电源键返回   方向键选择   OK 查看   长按 OK 操作", "電源 戻る   矢印 選択   OK 表示   OK長押し 操作"},
    {"POWER BACK   ARROWS STYLE   OK GENERATE / SAVE", "电源键返回   方向键选风格   OK 生成 / 保存", "電源 戻る   矢印 スタイル   OK 生成 / 保存"},
    {"POWER BACK   HOLD M TALK   L/R PERSONA   U/D VOLUME", "电源键返回   长按 M 对话   左右换角色   上下调音量", "電源 戻る   M長押し 会話   左右 キャラ   上下 音量"},
    {"POWER BACK   M CONTROLS   UP/DOWN PHOTO   LEFT/RIGHT SELECT   OK ENTER", "电源键返回   M 控制   上下换照片   左右选择   OK 进入", "電源 戻る   M 操作   上下 写真   左右 選択   OK 決定"},
    {"POWER BACK   M LANGUAGE   L/R SELECT   OK START", "电源键返回   M 选择语言   左右切换   OK 开始", "電源 戻る   M 言語   左右 選択   OK 開始"},
    {"POWER BACK   U/D VOLUME   L/R LANGUAGE   OK SOURCE/TARGET", "电源键返回   上下调音量   左右切语言   OK 源/目标", "電源 戻る   上下 音量   左右 言語   OK 原文/翻訳先"},
    {"POWER SCENES   HOLD M TALK   L/R HISTORY   U/D VOLUME", "电源键返回场景   长按 M 对话   左右看记录   上下调音量", "電源 シーン   M長押し 会話   左右 履歴   上下 音量"},
    {"POWER OFF", "关机", "電源オフ"},
    {"CONFIRM SHUTDOWN", "确认关机", "シャットダウン確認"},
    {"KEEP RUNNING", "继续使用", "使用を続ける"},
    {"Power off the device?", "确认关机？", "電源を切りますか？"},
    {"Powering off...", "正在关机...", "電源を切っています..."},
    {"RAIN", "雨", "雨"},
    {"READY", "就绪", "準備完了"},
    {"READY  >  HOLD M TO SPEAK", "就绪  >  长按 M 键说话", "準備完了  >  Mを長押しして話す"},
    {"READY  >  TRANSLATION COMPLETE", "就绪  >  翻译完成", "準備完了  >  翻訳完了"},
    {"RECOGNIZED  >  BAIDU TRANSLATING", "识别完成  >  百度翻译中", "認識完了  >  Baidu翻訳中"},
    {"Recognizing source text...", "正在识别原文...", "原文を認識中..."},
    {"RECOGNIZING", "识别中", "認識中"},
    {"RELEASE M TO SEND", "松开 M 键发送", "Mを離して送信"},
    {"RELEASED  >  BAIDU ASR", "已松开  >  百度语音识别", "送信済み  >  Baidu音声認識"},
    {"REMOVE FROM SD CARD", "从 SD 卡删除", "SDカードから削除"},
    {"RESETTING STORAGE", "正在清除存储", "ストレージを消去中"},
    {"RESULT READY", "结果已就绪", "結果の準備完了"},
    {"RETURN TO SETTINGS", "返回设置", "設定に戻る"},
    {"RUN SELECTED ACTION", "执行所选操作", "選択した操作を実行"},
    {"Russian", "俄语", "ロシア語"},
    {"SAVE", "保存", "保存"},
    {"SAVE FAILED", "保存失败", "保存に失敗しました"},
    {"SAVING...", "保存中...", "保存中..."},
    {"SCANNING...", "扫描中...", "スキャン中..."},
    {"SCREEN BRIGHTNESS", "屏幕亮度", "画面の明るさ"},
    {"SD CARD", "SD 卡", "SDカード"},
    {"SD DATA", "SD 卡数据", "SDデータ"},
    {"SD STORAGE", "SD 卡容量", "SDカード容量"},
    {"FREE %u.%u GB / %u.%u GB", "剩余 %u.%u GB / 总计 %u.%u GB", "空き %u.%u GB / 合計 %u.%u GB"},
    {"SD READY  ·  JPEG PHOTOS", "SD 卡已就绪  ·  已发现 JPEG 照片", "SDカード準備完了  ·  JPEG写真あり"},
    {"SD NOT READY  ·  CHECK CARD", "SD 卡未就绪  ·  请检查", "SDカード未準備  ·  確認してください"},
    {"SD NOT SCANNED", "尚未扫描 SD 卡", "SDカード未スキャン"},
    {"SD READY  ·  NO JPEG PHOTOS", "SD 卡已就绪  ·  没有 JPEG 照片", "SDカード準備完了  ·  JPEG写真なし"},
    {"SEARCH", "搜索", "検索"},
    {"SEARCH CITY", "搜索城市", "都市を検索"},
    {"SEARCHING OPEN-METEO...", "正在搜索 Open-Meteo...", "Open-Meteoで検索中..."},
    {"SECURED", "已加密", "暗号化"},
    {"SELECT A LANGUAGE", "选择语言", "言語を選択"},
    {"SELECT A NETWORK TO CONNECT", "选择要连接的网络", "接続するネットワークを選択"},
    {"SELECT A PRESET OR SEARCH", "选择预设城市或搜索", "プリセットを選択または検索"},
    {"SELECT A STYLE", "选择风格", "スタイルを選択"},
    {"SETTINGS", "设置", "設定"},
    {"SETTINGS / LOCATION", "设置 / 位置", "設定 / 地域"},
    {"SETTINGS / LOCATION SEARCH", "设置 / 位置搜索", "設定 / 地域検索"},
    {"SETTINGS / WI-FI", "设置 / Wi-Fi", "設定 / Wi-Fi"},
    {"SHIFT", "大写", "シフト"},
    {"SLIDESHOW", "幻灯片", "スライドショー"},
    {"SLIDESHOW UNAVAILABLE", "幻灯片不可用", "スライドショーを利用できません"},
    {"SNOW", "雪", "雪"},
    {"SOURCE CONFIRMED  >  SELECT TARGET", "源语言已确认  >  选择目标语言", "原文言語を確定  >  翻訳先を選択"},
    {"SOURCE LANGUAGE", "源语言", "原文言語"},
    {"SOURCE LANGUAGE SELECTED", "已选择源语言", "原文言語を選択しました"},
    {"SOURCE TEXT", "原文", "原文"},
    {"SPACE", "空格", "スペース"},
    {"SPANISH", "西班牙语", "スペイン語"},
    {"SPEAKING", "口语", "スピーキング"},
    {"SPEAKING PRACTICE", "对话练习", "会話練習"},
    {"STORAGE RESET", "存储清除", "ストレージ消去"},
    {"STORAGE RESET COMPLETE", "存储清除完成", "ストレージ消去完了"},
    {"STORAGE RESET FAILED", "存储清除失败", "ストレージ消去失敗"},
    {"STORAGE RESET IN PROGRESS", "正在清除存储", "ストレージ消去中"},
    {"STORYTELLER", "故事讲述者", "ストーリーテラー"},
    {"STRONG", "强", "強い"},
    {"STUDY MENTOR", "学习导师", "学習メンター"},
    {"SUBMIT", "提交", "送信"},
    {"Swedish", "瑞典语", "スウェーデン語"},
    {"SYNCING DATE", "正在同步日期", "日付を同期中"},
    {"TALK", "说话", "話す"},
    {"TARGET CONFIRMED  >  SELECT SOURCE", "目标语言已确认  >  选择源语言", "翻訳先を確定  >  原文言語を選択"},
    {"TARGET LANGUAGE", "目标语言", "翻訳先言語"},
    {"TARGET LANGUAGE SELECTED", "已选择目标语言", "翻訳先言語を選択しました"},
    {"Thai", "泰语", "タイ語"},
    {"THINKING", "思考中", "考え中"},
    {"THUNDERSTORM", "雷暴", "雷雨"},
    {"TRANSLATE", "翻译", "翻訳"},
    {"TRANSLATION", "译文", "翻訳"},
    {"TRANSLATION  >  PLAYING TARGET", "翻译完成  >  正在播放译文", "翻訳完了  >  翻訳音声を再生中"},
    {"TRAVELER", "旅行助手", "旅行者"},
    {"Turkish", "土耳其语", "トルコ語"},
    {"UNKNOWN", "未知", "不明"},
    {"UPDATING WEATHER", "正在更新天气", "天気を更新中"},
    {"UTC+8 CHINA STANDARD TIME", "UTC+8 中国标准时间", "UTC+8 中国標準時"},
    {"Vietnamese", "越南语", "ベトナム語"},
    {"VIRTUAL KEYBOARD", "虚拟键盘", "仮想キーボード"},
    {"VOICE COMPANION", "语音伙伴", "音声コンパニオン"},
    {"VOICE INPUT", "语音输入", "音声入力"},
    {"VOICE SERVICE ERROR", "语音服务错误", "音声サービスエラー"},
    {"WAITING FOR AI", "等待 AI", "AIを待っています"},
    {"Waiting for Baidu result...", "正在等待百度结果...", "Baiduの結果を待っています..."},
    {"Waiting for Baidu translation...", "正在等待百度翻译...", "Baidu翻訳を待っています..."},
    {"WAITING FOR BAIDU", "等待百度服务", "Baiduを待っています"},
    {"WAITING FOR NETWORK", "等待网络", "ネットワークを待っています"},
    {"WAITING FOR NETWORK SCAN", "等待网络扫描", "ネットワークスキャンを待っています"},
    {"WAITING FOR VOICE SERVICE", "等待语音服务", "音声サービスを待っています"},
    {"WAITING FOR WEATHER", "等待天气数据", "天気情報を待っています"},
    {"WATERCOLOR", "水彩", "水彩"},
    {"WEAK", "弱", "弱い"},
    {"WEATHER CITY", "天气城市", "天気の都市"},
    {"WEATHER HOME", "天气主页", "天気ホーム"},
    {"WEATHER LOCATION", "天气位置", "天気の地域"},
    {"WEATHER SYNCING", "正在同步天气", "天気を同期中"},
    {"WEATHER UPDATE FAILED", "天气更新失败", "天気の更新に失敗しました"},
    {"WELCOME", "欢迎", "ようこそ"},
    {"WAITING FOR PHOTO", "等待照片", "写真を待機中"},
    {"WELLNESS COACH", "健康教练", "ウェルネスコーチ"},
    {"WI-FI", "Wi-Fi", "Wi-Fi"},
    {"WI-FI NETWORKS", "Wi-Fi 网络", "Wi-Fiネットワーク"},
    {"WI-FI PASSWORD", "Wi-Fi 密码", "Wi-Fiパスワード"},
    {"YES", "是", "はい"},
    {"SELECT A SCENE AND LANGUAGE (A1/A2)", "选择对话场景和练习语言（A1/A2）", "シーンと言語を選択（A1/A2）"},
    {"DIALOGUE LOG", "对话记录", "対話ログ"},
    {"PRACTICE WITH: A NEW ACQUAINTANCE", "练习对象：刚认识的新朋友", "練習相手：新しく知り合った友達"},
    {"GOAL: GREET, INTRODUCE YOURSELF AND ANSWER SIMPLE QUESTIONS.", "练习目标：打招呼、自我介绍，并回答简单问题。", "目標：あいさつ、自己紹介、簡単な質問に答える。"},
    {"PRACTICE WITH: A HOTEL RECEPTIONIST", "练习对象：酒店前台", "練習相手：ホテルのフロント"},
    {"GOAL: GIVE BOOKING DETAILS AND CHECK IN.", "练习目标：说明预订信息，并完成简单入住交流。", "目標：予約情報を伝えてチェックインを完了する。"},
    {"PRACTICE WITH: A SHOP ASSISTANT", "练习对象：服装店员", "練習相手：服屋の店員"},
    {"GOAL: ASK ABOUT ITEMS, COLORS OR PRICES AND REPLY.", "练习目标：询问商品、颜色或价格，并回应店员。", "目標：商品、色、値段を尋ねて応対する。"},
    {"PRACTICE WITH: A RESTAURANT SERVER", "练习对象：餐厅服务员", "練習相手：レストランの店員"},
    {"GOAL: READ THE MENU, ORDER AND STATE SIMPLE NEEDS.", "练习目标：查看菜单、选择餐点，并说明简单需求。", "目標：メニューを見て注文し、簡単な要望を伝える。"},
    {"PRACTICE WITH: A HELPFUL LOCAL", "练习对象：热心的当地人", "練習相手：親切な地元の人"},
    {"GOAL: ASK FOR A PLACE AND FOLLOW SIMPLE DIRECTIONS.", "练习目标：询问地点，并听懂简单的方向说明。", "目標：場所を尋ね、簡単な道案内を理解する。"},
    {"PRACTICE WITH: A STATION CLERK", "练习对象：车站售票员", "練習相手：駅の窓口係"},
    {"GOAL: BUY A TICKET AND CONFIRM DESTINATION, TIME OR PRICE.", "练习目标：购买车票，并确认目的地、时间或价格。", "目標：切符を買い、行き先・時間・料金を確認する。"},
    {"Please wait. Do not remove storage.", "请稍候，不要移除存储设备。", "お待ちください。ストレージを取り外さないでください。"},
    {"Clear SD data? Font files will be preserved.", "清除 SD 卡数据？字模文件会被保留。", "SDデータを消去しますか？フォントは保持されます。"},
    {"Format Flash? This operation cannot be undone.", "格式化 Flash？此操作不可撤销。", "Flashをフォーマットしますか？この操作は元に戻せません。"},
    {"Clear SD data and format Flash? Fonts will be preserved.", "清除 SD 卡数据并格式化 Flash？字模会被保留。", "SDデータ消去とFlashフォーマットを行いますか？フォントは保持されます。"},
    {"SD data cleared. Font files were preserved.", "SD 卡数据已清除，字模文件已保留。", "SDデータを消去し、フォントを保持しました。"},
    {"Flash is ready to use.", "Flash 已可使用。", "Flashを使用できます。"},
    {"Storage reset complete. Font files were preserved.", "存储清除完成，字模文件已保留。", "ストレージ消去完了。フォントは保持されました。"},
    {"Storage was not fully reset. Check the media.", "存储未完全清除，请检查介质。", "ストレージを完全に消去できませんでした。メディアを確認してください。"},
};

static const ai_album_translation_t *i18n_find(const char *source)
{
    uint32_t index;

    if (source == NULL || source[0] == '\0') return NULL;
    for (index = 0U; index < sizeof(g_translations) /
                               sizeof(g_translations[0]); ++index) {
        if (strcmp(g_translations[index].english, source) == 0) {
            return &g_translations[index];
        }
    }
    return NULL;
}

const char *ai_album_i18n_text_for(ai_album_language_t language,
                                   const char *source)
{
    const ai_album_translation_t *translation = i18n_find(source);

    if (source == NULL) return "";
    if (translation == NULL) return source;
    if (language == AI_ALBUM_LANGUAGE_CHINESE_SIMPLIFIED) {
        return translation->chinese;
    }
    if (language == AI_ALBUM_LANGUAGE_JAPANESE) {
        return translation->japanese;
    }
    return translation->english;
}

const char *ai_album_i18n_text(const char *source)
{
    return ai_album_i18n_text_for(ai_album_language_get(), source);
}

static uint8_t i18n_label_matches(lv_obj_t *label, const lv_font_t *font,
                                  const char *text)
{
    const char *current = lv_label_get_text(label);

    return (uint8_t)(font != NULL &&
                     lv_obj_get_style_text_font(label, LV_PART_MAIN) == font &&
                     current != NULL && strcmp(current, text) == 0);
}

static void i18n_apply_label(lv_obj_t *label, ai_album_language_t language,
                             const char *text,
                             const lv_font_t *english_font)
{
    if (label == NULL || english_font == NULL) return;
    if (text == NULL) text = "";
    /* 渲染循环会反复下发同一段文案, 无变化时不再重排文本与字体 */
    if (language == AI_ALBUM_LANGUAGE_ENGLISH) {
        if (i18n_label_matches(label, english_font, text)) return;
        lv_obj_set_style_text_font(label, english_font, LV_PART_MAIN);
        lv_label_set_text(label, text);
        return;
    }
    (void)ai_album_font_set_dynamic_text(
        label, ai_album_language_locale(language), text);
}

void ai_album_i18n_set_label_text(lv_obj_t *label, const char *source,
                                  const lv_font_t *english_font)
{
    ai_album_language_t language = ai_album_language_get();

    i18n_apply_label(label, language,
                     ai_album_i18n_text_for(language, source), english_font);
}

void ai_album_i18n_set_label_raw(lv_obj_t *label, const char *text,
                                 const lv_font_t *english_font)
{
    i18n_apply_label(label, ai_album_language_get(), text, english_font);
}

void ai_album_i18n_set_label_native(lv_obj_t *label,
                                    ai_album_language_t language,
                                    const char *text,
                                    const lv_font_t *english_font)
{
    i18n_apply_label(label, language, text, english_font);
}
