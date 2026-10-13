#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWheelEvent>

#include "backend.h"
#include "markdownhighlighter.h"

class OmawriteTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody")),
                 QStringLiteral("My first draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B")), QStringLiteral("A-B.md"));
        QCOMPARE(Backend::suggestedFileName(QString()), QStringLiteral("Untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md")),
                 QStringLiteral("Already.md"));
    }

    void keepsNotoFromCrowdingTheFontList() {
        const QStringList fonts = Backend::selectableFontFamilies({
            QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Liberation Serif"),
            QStringLiteral("Noto Serif"), QStringLiteral("Noto Sans Tamil UI"),
            QStringLiteral("adwaita Sans"), QStringLiteral("IBM Plex Mono"),
            QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans Mono"),
            QStringLiteral("Liberation Serif"), QStringLiteral("Monospace"),
            QStringLiteral("Standard Symbols PS"), QStringLiteral("D050000L"),
            QStringLiteral("Noto Color Emoji"), QStringLiteral("Nimbus Sans [UKWN]"),
            QStringLiteral("Nimbus Sans [URW ]")});
        QCOMPARE(fonts, QStringList({QStringLiteral("IBM Plex Mono"),
                                     QStringLiteral("adwaita Sans"),
                                     QStringLiteral("Liberation Serif"),
                                     QStringLiteral("Nimbus Sans"),
                                     QStringLiteral("Noto Sans"),
                                     QStringLiteral("Noto Sans Mono"),
                                     QStringLiteral("Noto Serif")}));
    }

    void remembersEditorFont() {
        const QString installed = Backend().availableFonts().value(1);
        QVERIFY(!installed.isEmpty());

        {
            Backend backend;
            QCOMPARE(backend.editorFont(), Backend::defaultEditorFont());
            QSignalSpy fontSpy(&backend, &Backend::editorFontChanged);
            backend.setEditorFont(installed);
            QCOMPARE(fontSpy.count(), 1);
        }

        QCOMPARE(Backend().editorFont(), installed);

        // A remembered font that has since been uninstalled falls back.
        QSettings().setValue(QStringLiteral("editor/font"), QStringLiteral("No Such Font"));
        QCOMPARE(Backend().editorFont(), Backend::defaultEditorFont());
        QSettings().remove(QStringLiteral("editor/font"));
    }

    void restoresEditorZoom_data() {
        QTest::addColumn<bool>("persist");
        QTest::addColumn<QVariant>("saved");
        QTest::addColumn<qreal>("expected");
        QTest::newRow("absent") << true << QVariant() << qreal(20);
        QTest::newRow("empty") << true << QVariant(QString()) << qreal(20);
        QTest::newRow("invalid") << true << QVariant(QStringLiteral("invalid")) << qreal(20);
        QTest::newRow("below-range") << true << QVariant(7) << qreal(20);
        QTest::newRow("above-range") << true << QVariant(73) << qreal(20);
        QTest::newRow("fractional") << true << QVariant(25.5) << qreal(25.5);
        QTest::newRow("disabled") << false << QVariant(32) << qreal(20);
    }

    void restoresEditorZoom() {
        QFETCH(bool, persist);
        QFETCH(QVariant, saved);
        QFETCH(qreal, expected);
        QSettings settings;
        settings.setValue(QStringLiteral("editor/persistZoom"), persist);
        if (saved.isValid())
            settings.setValue(QStringLiteral("editor/zoom"), saved);
        else
            settings.remove(QStringLiteral("editor/zoom"));
        QCOMPARE(Backend().editorZoom(), expected);
        settings.remove(QStringLiteral("editor/persistZoom"));
        settings.remove(QStringLiteral("editor/zoom"));
    }

    void persistsZoomOnlyWhenEnabled() {
        QSettings settings;
        const QString persistKey = QStringLiteral("editor/persistZoom");
        const QString zoomKey = QStringLiteral("editor/zoom");
        {
            Backend backend;
            backend.setEditorZoom(24);
            QVERIFY(!QSettings().contains(zoomKey));
        }
        settings.setValue(persistKey, true);
        {
            Backend backend;
            backend.setEditorZoom(25.5);
            QCOMPARE(QSettings().value(zoomKey).toDouble(), 25.5);
            backend.setEditorZoom(32);
            QCOMPARE(QSettings().value(zoomKey).toDouble(), 32.0);
        }
        QCOMPARE(Backend().editorZoom(), 32.0);
        settings.setValue(persistKey, false);
        {
            Backend backend;
            backend.setEditorZoom(40);
            QCOMPARE(QSettings().value(zoomKey).toDouble(), 32.0);
        }
        QCOMPARE(Backend().editorZoom(), 20.0);
        settings.remove(persistKey);
        settings.remove(zoomKey);
    }

    void scalesMarginsWithZoom_data() {
        QTest::addColumn<bool>("enabled");
        QTest::newRow("proportional") << true;
        QTest::newRow("fixed") << false;
    }

    void scalesMarginsWithZoom() {
        QFETCH(bool, enabled);
        QSettings settings;
        const QString key = QStringLiteral("editor/scaleMarginsWithZoom");
        settings.setValue(key, enabled);
        {
            Backend backend;
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
            QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../src/Main.qml")));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            QScopedPointer<QObject> root(component.create());
            QVERIFY2(root, qPrintable(component.errorString()));
            auto *window = qobject_cast<QQuickWindow *>(root.data());
            QVERIFY(window);
            QVERIFY(QTest::qWaitForWindowExposed(window));
            window->setWidth(1280);
            QCoreApplication::processEvents();
            auto *editor = window->findChild<QQuickItem *>(QStringLiteral("sourceEditor"));
            QVERIFY(editor);
            const qreal left = editor->mapToScene(QPointF()).x();
            const int footerHeight = window->property("footerHeight").toInt();
            backend.setEditorZoom(22);
            QCoreApplication::processEvents();
            QVERIFY(qAbs(editor->mapToScene(QPointF()).x() - left * (enabled ? 1.1 : 1)) <= 1);
            QCOMPARE(window->property("footerHeight").toInt(), footerHeight);
            backend.setEditorZoom(72);
            window->setWidth(720);
            QCoreApplication::processEvents();
            QVERIFY(editor->width() >= 360);
            QVERIFY(editor->mapToScene(QPointF()).x() >= 0);
            QVERIFY(editor->mapToScene(QPointF(editor->width(), 0)).x() <= window->width());
        }
        settings.remove(key);
        settings.remove(QStringLiteral("window"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com)"));
        QCOMPARE(markup.size(), 3);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("renderedPreview")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("modeToggle")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void resizesEditorFontWithControlWheel() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        auto *editor = window->findChild<QQuickItem *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QQuickItem *flick = editor->parentItem()->parentItem();
        const QString text = QStringLiteral("A line of Markdown text.\n").repeated(200);
        editor->setProperty("text", text);
        QVERIFY(QMetaObject::invokeMethod(editor, "select",
                                         Q_ARG(int, 2), Q_ARG(int, 9)));
        const int cursor = editor->property("cursorPosition").toInt();
        const bool modified = backend.modified();
        const int footerHeight = window->property("footerHeight").toInt();
        const auto fontSize = [editor] {
            return editor->property("font").value<QFont>().pixelSize();
        };
        const auto wheel = [window, flick](QPoint angle, QPoint pixels = {},
                                          Qt::KeyboardModifiers modifiers = Qt::ControlModifier) {
            const QPointF position = flick->mapToScene(
                QPointF(flick->width() / 2, flick->height() / 2));
            QWheelEvent event(position, window->mapToGlobal(position), pixels, angle,
                              Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(window, &event);
        };

        const int initialSize = fontSize();
        wheel(QPoint(0, 120));
        QCOMPARE(fontSize(), initialSize + 2);
        wheel(QPoint(0, -120));
        QCOMPARE(fontSize(), initialSize);
        // Fractional notches accumulate rather than being discarded.
        wheel(QPoint(0, 30));
        wheel(QPoint(0, 30));
        QCOMPARE(fontSize(), initialSize + 1);
        wheel(QPoint(0, -60));
        QCOMPARE(fontSize(), initialSize);
        // Pixel-precise touchpad events use the same zoom path.
        wheel({}, QPoint(0, 60));
        QCOMPARE(fontSize(), initialSize + 2);
        wheel({}, QPoint(0, -60));
        QCOMPARE(fontSize(), initialSize);
        wheel(QPoint(120, 0));
        wheel({});
        QCOMPARE(fontSize(), initialSize);
        QTest::qWait(250);
        QCOMPARE(flick->property("contentY").toReal(), 0.0);
        QCOMPARE(window->property("footerHeight").toInt(), footerHeight);
        QCOMPARE(editor->property("text").toString(), text);
        QCOMPARE(editor->property("cursorPosition").toInt(), cursor);
        QCOMPARE(editor->property("selectionStart").toInt(), 2);
        QCOMPARE(editor->property("selectionEnd").toInt(), 9);
        QCOMPARE(backend.modified(), modified);

        wheel(QPoint(0, -120), {}, Qt::NoModifier);
        QTRY_VERIFY(flick->property("contentY").toReal() > 0);
        QCOMPARE(fontSize(), initialSize);
        // Switching to zoom cancels a still-running scroll animation.
        wheel(QPoint(0, 120));
        const qreal zoomContentY = flick->property("contentY").toReal();
        QTest::qWait(250);
        QCOMPARE(flick->property("contentY").toReal(), zoomContentY);

        wheel(QPoint(0, 120 * 100));
        QCOMPARE(fontSize(), 72);
        wheel(QPoint(0, 120));
        QCOMPARE(fontSize(), 72);
        wheel(QPoint(0, -120 * 100));
        QCOMPARE(fontSize(), 8);
        wheel(QPoint(0, -120));
        QCOMPARE(fontSize(), 8);
        wheel(QPoint(0, 120));
        QCOMPARE(fontSize(), 10);
        backend.setTextScale(1.5);
        QCOMPARE(fontSize(), 15);
        wheel(QPoint(0, 120));
        QCOMPARE(fontSize(), 18);

        // Other input methods can adjust zoom without constructing wheel events.
        QVERIFY(QMetaObject::invokeMethod(window, "adjustZoom",
                                         Q_ARG(QVariant, QVariant(1.0))));
        QCOMPARE(fontSize(), 21);
        QVERIFY(QMetaObject::invokeMethod(window, "adjustZoom",
                                         Q_ARG(QVariant, QVariant(-1.0))));
        QCOMPARE(fontSize(), 18);
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QStringLiteral("Untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmawriteTest)
#include "tst_omawrite.moc"
