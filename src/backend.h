#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QThread;

class Backend final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString themeBackground READ themeBackground NOTIFY themeChanged)
    Q_PROPERTY(QString themeForeground READ themeForeground NOTIFY themeChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent NOTIFY themeChanged)
    Q_PROPERTY(QString themeSelection READ themeSelection NOTIFY themeChanged)
    Q_PROPERTY(QStringList availableFonts READ availableFonts CONSTANT)
    Q_PROPERTY(QString editorFontFamily READ editorFontFamily NOTIFY editorFontChanged)
    Q_PROPERTY(int editorFontSize READ editorFontSize NOTIFY editorFontChanged)
    Q_PROPERTY(int editorSideMargin READ editorSideMargin NOTIFY editorSideMarginChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    QString themeBackground() const { return m_background; }
    QString themeForeground() const { return m_foreground; }
    QString themeAccent() const { return m_accent; }
    QString themeSelection() const { return m_selection; }
    QStringList availableFonts() const { return m_availableFonts; }
    QString editorFontFamily() const { return m_editorFontFamily; }
    int editorFontSize() const { return m_editorFontSize; }
    int editorSideMargin() const { return m_editorSideMargin; }

    Q_INVOKABLE QVariantMap render(const QString &source, const QString &color, int pixelSize);
    Q_INVOKABLE void renderAsync(int requestId, const QString &source,
                                 const QString &color, int pixelSize);
    Q_INVOKABLE QVariantMap loadDocument(const QString &urlOrPath);
    Q_INVOKABLE bool saveDocumentData(const QString &urlOrPath,
                                      const QVariantMap &document);
    Q_INVOKABLE bool saveDocument(const QString &urlOrPath, const QString &title,
                                  const QVariantList &lines);
    Q_INVOKABLE QVariantMap loadRecovery();
    Q_INVOKABLE void saveRecoveryData(const QVariantMap &document);
    Q_INVOKABLE void saveRecovery(const QString &title, const QVariantList &lines);
    Q_INVOKABLE bool saveSnapshot(const QVariantMap &document);
    Q_INVOKABLE QVariantList searchCourse(const QString &documentUrl,
                                          const QString &query) const;
    Q_INVOKABLE QVariantMap importAsset(const QString &documentUrl,
                                        const QString &sourceUrl) const;
    Q_INVOKABLE QVariantMap importClipboardImage(const QString &documentUrl) const;
    Q_INVOKABLE QVariantMap importPdf(const QString &documentUrl,
                                      const QString &sourceUrl) const;
    Q_INVOKABLE QVariantMap newFigureAsset(const QString &documentUrl) const;
    Q_INVOKABLE bool saveFigure(const QString &path, const QString &actionsJson,
                                int width, int height, const QString &background,
                                const QString &foreground, const QString &fontFamily) const;
    Q_INVOKABLE QString fileUrl(const QString &path) const;
    Q_INVOKABLE QString latexHint(const QString &source,
                                  const QString &compilerError) const;
    Q_INVOKABLE void setEditorFont(const QString &family, int pixelSize);
    Q_INVOKABLE void setEditorSideMargin(int margin);
    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE void setClipboardText(const QString &text);
    Q_INVOKABLE QVariantMap exportTex(const QString &urlOrPath, const QString &title,
                                      const QVariantList &lines);
    Q_INVOKABLE QVariantMap exportPdf(const QString &urlOrPath, const QString &title,
                                      const QVariantList &lines);

signals:
    void themeChanged();
    void editorFontChanged();
    void editorSideMarginChanged();
    void renderFinished(int requestId, const QVariantMap &result);
    void renderRequested(int requestId, const QString &source,
                         const QString &color, int pixelSize);

private:
    QString localPath(const QString &urlOrPath) const;
    QString recoveryPath() const;
    QString firstLatexError(const QByteArray &output) const;
    void loadTheme();
    void watchTheme();

    QFileSystemWatcher m_themeWatcher;
    QString m_background = QStringLiteral("#101010");
    QString m_foreground = QStringLiteral("#eeeeee");
    QString m_accent = QStringLiteral("#5584aa");
    QString m_selection = QStringLiteral("#186a9a");
    QStringList m_availableFonts;
    QString m_editorFontFamily;
    int m_editorFontSize = 17;
    int m_editorSideMargin = 100;
    QThread *m_renderThread = nullptr;
    QObject *m_renderWorker = nullptr;
};
