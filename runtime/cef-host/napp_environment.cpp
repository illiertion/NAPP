#include "napp_environment.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace napp::runtime {
namespace {

QString accentHex(const QString& accent)
{
    if (accent.compare(QStringLiteral("violet"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("#8b5cf6");
    if (accent.compare(QStringLiteral("mint"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("#1fc4a4");
    if (accent.compare(QStringLiteral("amber"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("#e9a23b");
    return QStringLiteral("#3d7df0");
}

// Only "</script>" can terminate the inline block early. The JSON encoder keeps
// the payload valid UTF-8; the replace keeps the document structure intact.
QString scriptSafe(QString json)
{
    json.replace(QStringLiteral("</"), QStringLiteral("<\\/"));
    return json;
}

} // namespace

QString NappEnvironment::accentColor() const
{
    return accentHex(accent);
}

QByteArray environmentJson(const NappEnvironment& environment)
{
    QJsonObject root;
    root.insert(QStringLiteral("apiVersion"), environment.apiVersion);
    root.insert(QStringLiteral("appId"), environment.appId);
    root.insert(QStringLiteral("appName"), environment.appName);
    root.insert(QStringLiteral("language"), environment.language);
    root.insert(QStringLiteral("theme"), environment.isDark() ? QStringLiteral("dark")
                                                              : QStringLiteral("light"));
    root.insert(QStringLiteral("accent"), environment.accent);
    root.insert(QStringLiteral("accentColor"), environment.accentColor());
    root.insert(QStringLiteral("repositoryUrl"), environment.repositoryUrl);
    root.insert(QStringLiteral("runtimeVersion"), environment.runtimeVersion);
    root.insert(QStringLiteral("platform"), QStringLiteral("windows-x64"));
    root.insert(QStringLiteral("exportPath"),
                QStringLiteral("/__host/export/<file>"));
    root.insert(QStringLiteral("pickFolderPath"), QStringLiteral("/__host/pick-folder"));
    root.insert(QStringLiteral("notifyPath"), QStringLiteral("/__host/notify"));
    root.insert(QStringLiteral("permissions"), QJsonArray::fromStringList(environment.permissions));
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray environmentScript(const NappEnvironment& environment)
{
    const QString json = scriptSafe(QString::fromUtf8(environmentJson(environment)));
    const bool dark = environment.isDark();

    // Apps get the launcher palette as CSS variables and a ready-made
    // `data-napp-theme` attribute; nothing here overrides an app's own styles.
    const QString css = QStringLiteral(
        ":root{color-scheme:%1;"
        "--napp-accent:%2;"
        "--napp-bg:%3;--napp-surface:%4;--napp-surface-alt:%5;"
        "--napp-border:%6;--napp-text:%7;--napp-muted:%8;}")
                            .arg(dark ? QStringLiteral("dark") : QStringLiteral("light"),
                                 environment.accentColor(),
                                 dark ? QStringLiteral("#080a0e") : QStringLiteral("#f4f6fa"),
                                 dark ? QStringLiteral("#12151b") : QStringLiteral("#ffffff"),
                                 dark ? QStringLiteral("#181c24") : QStringLiteral("#eef1f7"),
                                 dark ? QStringLiteral("#222833") : QStringLiteral("#dde3ed"),
                                 dark ? QStringLiteral("#f3f6fb") : QStringLiteral("#0b0e13"),
                                 dark ? QStringLiteral("#7f8999") : QStringLiteral("#5b6472"));

    QString script;
    script += QStringLiteral("<style data-napp-environment>%1</style>").arg(css);
    // The page-side Notification API is replaced by a shim that posts to the host,
    // which shows a real OS notification. Chromium never draws its own toast or
    // permission bubble. Only apps that declared the "notifications" permission
    // (and were granted it) get notifications; everyone else sees "denied".
    // 페이지의 Notification API 를 호스트로 보내는 shim 으로 교체한다. 호스트가 진짜 OS 알림을
    // 띄우고, Chromium 은 자체 토스트/권한 말풍선을 그리지 않는다. "notifications" 권한을
    // 선언하고 승인받은 앱만 알림을 보낼 수 있고, 나머지는 "denied" 로 보인다.
    script += QStringLiteral(
        "<script data-napp-environment>(function(){"
        "var info=%1;"
        "var granted=info.permissions.indexOf('notifications')>=0;"
        "var endpoint='napp://'+info.appId+'/__host/notify';"
        "function send(t,b){try{fetch(endpoint,{method:'POST',"
        "body:JSON.stringify({title:String(t==null?'':t),body:String(b==null?'':b)})})"
        ".catch(function(){});}catch(e){}}"
        "function NappNotification(title,options){options=options||{};"
        "this.title=String(title);this.body=options.body?String(options.body):'';"
        "this.onclick=null;this.onclose=null;this.onshow=null;this.onerror=null;"
        "this.close=function(){};this.addEventListener=function(){};"
        "this.removeEventListener=function(){};"
        "if(granted)send(this.title,this.body);}"
        "Object.defineProperty(NappNotification,'permission',"
        "{get:function(){return granted?'granted':'denied';}});"
        "NappNotification.requestPermission=function(cb){"
        "var r=granted?'granted':'denied';if(typeof cb==='function')cb(r);"
        "return Promise.resolve(r);};"
        "try{Object.defineProperty(window,'Notification',"
        "{value:NappNotification,configurable:true,writable:true});}catch(e){}"
        "window.napp=Object.freeze({"
        "apiVersion:info.apiVersion,appId:info.appId,appName:info.appName,"
        "language:info.language,theme:info.theme,accent:info.accent,accentColor:info.accentColor,"
        "repositoryUrl:info.repositoryUrl,runtimeVersion:info.runtimeVersion,"
        "platform:info.platform,permissions:info.permissions,exportPath:info.exportPath,"
        "pickFolderPath:info.pickFolderPath,"
        "notify:send,notificationsGranted:granted,"
        "isDark:info.theme==='dark'});"
        "try{var root=document.documentElement;"
        "root.setAttribute('data-napp-theme',info.theme);"
        "root.setAttribute('data-napp-language',info.language);"
        "root.style.colorScheme=info.theme;"
        "document.dispatchEvent(new CustomEvent('napp:environment',{detail:window.napp}));"
        "}catch(e){}"
        "})();</script>")
                      .arg(json);
    return script.toUtf8();
}

QByteArray injectEnvironment(const QByteArray& html, const NappEnvironment& environment)
{
    const QByteArray script = environmentScript(environment);
    if (html.isEmpty())
        return html;

    // Case-insensitive search for the closing head tag.
    const QByteArray lower = html.toLower();
    const int head = lower.indexOf("</head>");
    if (head >= 0) {
        QByteArray result = html;
        result.insert(head, script);
        return result;
    }

    const int body = lower.indexOf("<body");
    if (body >= 0) {
        const int close = html.indexOf('>', body);
        if (close > 0) {
            QByteArray result = html;
            result.insert(close + 1, script);
            return result;
        }
    }

    QByteArray result = script;
    result.append(html);
    return result;
}

} // namespace napp::runtime
