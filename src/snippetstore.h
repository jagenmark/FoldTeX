#pragma once

#include <QVariantList>
#include <QVariantMap>

class SnippetStore final {
public:
    QVariantList entries() const;
    QVariantMap replace(const QVariantList &entries) const;
    QVariantMap resolve(const QString &trigger, const QString &course) const;
    QVariantMap importFile(const QString &path) const;
    QVariantMap exportFile(const QString &path) const;

private:
    void load() const;
    QVariantMap normalize(const QVariantMap &entry, QString *error) const;
    QVariantMap validate(const QVariantList &entries, QVariantList *normalized) const;
    mutable bool m_loaded = false;
    mutable QVariantList m_entries;
};
