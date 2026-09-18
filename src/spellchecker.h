#pragma once
#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QStringList>
#include <atomic>
#include <memory>

// All dictionary work runs on one worker thread. Nothing leaves this process.
class SpellChecker : public QObject {
    Q_OBJECT
public:
    explicit SpellChecker(QObject *parent = nullptr);
    ~SpellChecker() override;
    void check(const QVariantList &rows, const QString &language, const QStringList &ignored);
    void cancel();
    void suggest(int request, const QString &word, const QString &language);
    static QVariantList words(const QVariantList &rows);
signals:
    void checked(const QVariantList &issues, const QString &error);
    void suggested(int request, const QStringList &suggestions);
private:
    struct Dictionaries;
    std::unique_ptr<Dictionaries> m_dictionaries;
    QThread m_thread;
    QObject *m_worker;
    std::atomic<int> m_generation{0};
};
