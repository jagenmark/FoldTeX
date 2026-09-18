#include "snippetstore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QUuid>

namespace {
constexpr auto settingsKey = "snippets/custom";

QStringList cleanList(const QVariant &value, bool lowercase) {
    QStringList source;
    if (value.metaType().id() == QMetaType::QString)
        source = value.toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    else
        source = value.toStringList();
    QStringList result;
    QSet<QString> seen;
    for (QString item : source) {
        item = item.trimmed();
        if (lowercase)
            item = item.toLower();
        const QString key = item.toLower();
        if (!item.isEmpty() && !seen.contains(key)) {
            result.append(item);
            seen.insert(key);
        }
    }
    return result;
}

QSet<QString> tokens(const QVariantMap &entry) {
    QSet<QString> result;
    result.insert(entry.value(QStringLiteral("trigger")).toString().toLower());
    const QStringList aliases = entry.value(QStringLiteral("aliases")).toStringList();
    for (const QString &alias : aliases)
        result.insert(alias.toLower());
    return result;
}

bool scopesOverlap(const QVariantMap &left, const QVariantMap &right) {
    if (left.value(QStringLiteral("allCourses")).toBool()
        || right.value(QStringLiteral("allCourses")).toBool())
        return true;
    QSet<QString> leftCourses;
    for (const QString &course : left.value(QStringLiteral("courses")).toStringList())
        leftCourses.insert(course.toLower());
    for (const QString &course : right.value(QStringLiteral("courses")).toStringList()) {
        if (leftCourses.contains(course.toLower()))
            return true;
    }
    return false;
}
}

void SnippetStore::load() const {
    if (m_loaded)
        return;
    const QByteArray stored = QSettings().value(QString::fromLatin1(settingsKey)).toByteArray();
    const QJsonDocument document = QJsonDocument::fromJson(stored);
    m_entries = document.isArray() ? document.array().toVariantList() : QVariantList{};
    m_loaded = true;
}

QVariantList SnippetStore::entries() const {
    load();
    return m_entries;
}

QVariantMap SnippetStore::normalize(const QVariantMap &entry, QString *error) const {
    static const QRegularExpression triggerPattern(QStringLiteral("^[A-Za-z][A-Za-z0-9]*$"));
    QVariantMap result;
    QString trigger = entry.value(QStringLiteral("trigger")).toString().trimmed().toLower();
    if (!triggerPattern.match(trigger).hasMatch()) {
        *error = QStringLiteral("Triggers must start with a letter and contain only letters and numbers");
        return {};
    }
    const QString expansion = entry.value(QStringLiteral("template")).toString();
    if (expansion.isEmpty()) {
        *error = QStringLiteral("The expansion cannot be empty");
        return {};
    }
    QStringList aliases = cleanList(entry.value(QStringLiteral("aliases")), true);
    aliases.removeAll(trigger);
    for (const QString &alias : aliases) {
        if (!triggerPattern.match(alias).hasMatch()) {
            *error = QStringLiteral("Alias '%1' must start with a letter and contain only letters and numbers")
                         .arg(alias);
            return {};
        }
    }
    const bool allCourses = !entry.contains(QStringLiteral("allCourses"))
        || entry.value(QStringLiteral("allCourses")).toBool();
    const QStringList courses = cleanList(entry.value(QStringLiteral("courses")), false);
    if (!allCourses && courses.isEmpty()) {
        *error = QStringLiteral("Choose at least one course or use All courses");
        return {};
    }
    QString id = entry.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty())
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = entry.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
        name = trigger;
    result.insert(QStringLiteral("id"), id);
    result.insert(QStringLiteral("name"), name);
    result.insert(QStringLiteral("trigger"), trigger);
    result.insert(QStringLiteral("aliases"), aliases);
    result.insert(QStringLiteral("template"), expansion);
    result.insert(QStringLiteral("allCourses"), allCourses);
    result.insert(QStringLiteral("courses"), courses);
    result.insert(QStringLiteral("enabled"), !entry.contains(QStringLiteral("enabled"))
                      || entry.value(QStringLiteral("enabled")).toBool());
    return result;
}

QVariantMap SnippetStore::validate(const QVariantList &entries,
                                   QVariantList *normalized) const {
    normalized->clear();
    QSet<QString> ids;
    for (const QVariant &value : entries) {
        QString error;
        QVariantMap entry = normalize(value.toMap(), &error);
        if (!error.isEmpty())
            return {{QStringLiteral("saved"), false}, {QStringLiteral("error"), error}};
        if (ids.contains(entry.value(QStringLiteral("id")).toString()))
            entry.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        ids.insert(entry.value(QStringLiteral("id")).toString());
        normalized->append(entry);
    }
    for (qsizetype i = 0; i < normalized->size(); ++i) {
        const QVariantMap left = normalized->at(i).toMap();
        if (!left.value(QStringLiteral("enabled")).toBool())
            continue;
        const QSet<QString> leftTokens = tokens(left);
        for (qsizetype j = i + 1; j < normalized->size(); ++j) {
            const QVariantMap right = normalized->at(j).toMap();
            if (!right.value(QStringLiteral("enabled")).toBool()
                || !scopesOverlap(left, right))
                continue;
            QSet<QString> overlap = leftTokens;
            overlap.intersect(tokens(right));
            if (!overlap.isEmpty()) {
                return {{QStringLiteral("saved"), false},
                        {QStringLiteral("error"),
                         QStringLiteral("Trigger or alias '%1' is used by overlapping snippets")
                             .arg(*overlap.constBegin())}};
            }
        }
    }
    return {{QStringLiteral("saved"), true}, {QStringLiteral("error"), QString()}};
}

QVariantMap SnippetStore::replace(const QVariantList &newEntries) const {
    QVariantList normalized;
    QVariantMap result = validate(newEntries, &normalized);
    if (!result.value(QStringLiteral("saved")).toBool())
        return result;
    QSettings().setValue(QString::fromLatin1(settingsKey),
                         QJsonDocument::fromVariant(normalized).toJson(QJsonDocument::Compact));
    m_entries = normalized;
    m_loaded = true;
    result.insert(QStringLiteral("entries"), normalized);
    return result;
}

QVariantMap SnippetStore::resolve(const QString &rawTrigger, const QString &course) const {
    const QString trigger = rawTrigger.trimmed().toLower();
    const QString wantedCourse = course.trimmed().toLower();
    for (const QVariant &value : entries()) {
        const QVariantMap entry = value.toMap();
        if (!entry.value(QStringLiteral("enabled"), true).toBool()
            || !tokens(entry).contains(trigger))
            continue;
        bool applies = entry.value(QStringLiteral("allCourses"), true).toBool();
        if (!applies) {
            for (const QString &candidate : entry.value(QStringLiteral("courses")).toStringList()) {
                if (candidate.trimmed().toLower() == wantedCourse) {
                    applies = true;
                    break;
                }
            }
        }
        if (applies) {
            QVariantMap result = entry;
            result.insert(QStringLiteral("found"), true);
            return result;
        }
    }
    return {{QStringLiteral("found"), false}};
}

QVariantMap SnippetStore::importFile(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {{QStringLiteral("saved"), false}, {QStringLiteral("error"), file.errorString()}};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()
        || document.object().value(QStringLiteral("format")).toString()
               != QStringLiteral("foldtex-snippets-1")
        || !document.object().value(QStringLiteral("snippets")).isArray()) {
        return {{QStringLiteral("saved"), false},
                {QStringLiteral("error"), QStringLiteral("Not a FoldTeX snippet file")}};
    }
    return replace(document.object().value(QStringLiteral("snippets")).toArray().toVariantList());
}

QVariantMap SnippetStore::exportFile(const QString &path) const {
    QJsonObject object;
    object.insert(QStringLiteral("format"), QStringLiteral("foldtex-snippets-1"));
    object.insert(QStringLiteral("snippets"), QJsonArray::fromVariantList(entries()));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        return {{QStringLiteral("saved"), false},
                {QStringLiteral("error"), QStringLiteral("Could not write the snippet file")}};
    }
    return {{QStringLiteral("saved"), true}, {QStringLiteral("error"), QString()}};
}
