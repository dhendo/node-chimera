#include "chimera.h"

WebPage::WebPage(QObject *parent)
    : QWebPage(parent)
{
    m_userAgent = QWebPage::userAgentForUrl(QUrl());
}

bool WebPage::go(int delta)
{
    int index = this->history()->currentItemIndex() + delta;
    QWebHistoryItem item = this->history()->itemAt(index);
    if (item.isValid()) {
        this->history()->goToItem(item);
        return true;
    }
    return false;
}

void WebPage::javaScriptAlert(QWebFrame *originatingFrame, const QString &msg)
{
    Q_UNUSED(originatingFrame);
    std::cout << "JavaScript alert: " << qPrintable(msg) << std::endl;
}

void WebPage::javaScriptConsoleMessage(const QString &message, int lineNumber, const QString &sourceID)
{
    if (!sourceID.isEmpty())
        std::cout << "webkit -- " << qPrintable(sourceID) << ":" << lineNumber << " " << std::endl;
    std::cout << "webkit -- "  << qPrintable(message) << std::endl;
}

bool WebPage::shouldInterruptJavaScript()
{
    QApplication::processEvents(QEventLoop::AllEvents, 42);
    return false;
}

QString WebPage::userAgentForUrl(const QUrl &url) const
{
    Q_UNUSED(url);
    return m_userAgent;
}

void WebPage::sendEvent(const QString &type, const QVariant &arg1, const QVariant &arg2)
{
    if (type == "mousedown" ||  type == "mouseup" || type == "mousemove") {
        QMouseEvent::Type eventType = QEvent::None;
        Qt::MouseButton button = Qt::LeftButton;
        Qt::MouseButtons buttons = Qt::LeftButton;

        if (type == "mousedown")
            eventType = QEvent::MouseButtonPress;
        if (type == "mouseup")
            eventType = QEvent::MouseButtonRelease;
        if (type == "mousemove") {
            eventType = QEvent::MouseMove;
            button = Qt::NoButton;
            buttons = Qt::NoButton;
        }
        Q_ASSERT(eventType != QEvent::None);

        int x = arg1.toInt();
        int y = arg2.toInt();
        QMouseEvent *event = new QMouseEvent(eventType, QPoint(x, y), button, buttons, Qt::NoModifier);
        QApplication::postEvent(this, event);
        QApplication::processEvents();
        return;
    }

    if (type == "click") {
        sendEvent("mousedown", arg1, arg2);
        sendEvent("mouseup", arg1, arg2);
        return;
    }
}

Chimera::Chimera(QObject *parent)
    : QObject(parent)
    , m_returnValue(0)
{
    QPalette palette = m_page.palette();
    palette.setBrush(QPalette::Base, Qt::transparent);
    m_page.setPalette(palette);
    m_page.setParent(this);
    
    connect(m_page.mainFrame(), SIGNAL(javaScriptWindowObjectCleared()), SLOT(inject()));
    connect(&m_page, SIGNAL(loadFinished(bool)), this, SLOT(finish(bool)));

    m_jar.setParent(this);
    m_page.networkAccessManager()->setCookieJar(&m_jar);

    m_page.settings()->setMaximumPagesInCache(3);
    m_page.settings()->setAttribute(QWebSettings::PluginsEnabled, true);
    m_page.settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, true);
    m_page.settings()->setAttribute(QWebSettings::OfflineStorageDatabaseEnabled, true);
    m_page.settings()->setAttribute(QWebSettings::LocalStorageEnabled, true);
    m_page.settings()->setLocalStoragePath(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
    m_page.settings()->setOfflineStoragePath(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
    m_page.setViewportSize(QSize(1024, 768));

    // Ensure we have document.body.
    m_page.mainFrame()->setHtml("<html><body></body></html>");

    m_page.mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    m_page.mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
}

QString Chimera::content() const
{
    return m_page.mainFrame()->toHtml();
}

void Chimera::disableImages()
{
  m_page.settings()->setAttribute(QWebSettings::AutoLoadImages, false);
}

void Chimera::setContent(const QString &content)
{
    m_page.mainFrame()->setHtml(content);
}

void Chimera::setLibraryCode(const QString &content)
{
  m_libraryCode = content;
}

void Chimera::setCookies(const QString &content)
{
  m_jar.setCookies(content);
}

void Chimera::setProxy(const QString &type, const QString &host, int port, const QString &username, const QString &password)
{
  QNetworkProxy proxy;
  if (type == "socks") {
    proxy.setType(QNetworkProxy::Socks5Proxy);
  } else {
    proxy.setType(QNetworkProxy::HttpProxy);
  }
  proxy.setHostName(host);
  proxy.setPort(port);
  proxy.setUser(username);
  proxy.setPassword(password);
  QNetworkAccessManager* manager = m_page.networkAccessManager();
  manager->setProxy(proxy);
}

QString Chimera::getCookies()
{
  return m_jar.getCookies();
}

void Chimera::setEmbedScript(const QString &jscode)
{
    m_script = jscode;
}

void Chimera::callback(const QString &errorResult, const QString &result)
{
  m_errors.enqueue(errorResult);
  m_results.enqueue(result);
  m_mutex.unlock();
  m_loading.wakeAll();
}

void Chimera::sendEvent(const QString &type, const QVariant &arg1, const QVariant &arg2)
{
  m_page.sendEvent(type, arg1, arg2);
}

void Chimera::exit(int code)
{
    m_page.triggerAction(QWebPage::Stop);
    m_returnValue = code;
    disconnect(&m_page, SIGNAL(loadFinished(bool)), this, SLOT(finish(bool)));
}

void Chimera::execute()
{
    std::cout << "debug -- about to lock" << std::endl;
    m_mutex.tryLock();
    std::cout << "debug -- about to evaluate" << std::endl;
    m_page.mainFrame()->evaluateJavaScript(m_script);
    std::cout << "debug -- done evaluating" << std::endl;
}

void Chimera::finish(bool success)
{
    m_loadStatus = success ? "success" : "fail";
    m_page.mainFrame()->evaluateJavaScript(m_libraryCode);
    m_page.mainFrame()->evaluateJavaScript(m_script);
}

void Chimera::inject()
{
    m_page.mainFrame()->addToJavaScriptWindowObject("chimera", this);
}

QString Chimera::loadStatus() const
{
    return m_loadStatus;
}

void Chimera::open(const QString &address)
{
    m_page.triggerAction(QWebPage::Stop);
    m_loadStatus = "loading";
    m_mutex.lock();
    m_page.mainFrame()->setUrl(QUrl(address));
}
void Chimera::renderSnippet(const QString &html)
{
    m_page.triggerAction(QWebPage::Stop);
    m_loadStatus = "loading";
    m_mutex.lock();
    m_page.mainFrame()->setHtml(html);
}

void Chimera::wait()
{
  if (m_mutex.tryLock()) {
    m_mutex.unlock();
  } else {
    m_loading.wait(&m_mutex);
  }
}

bool Chimera::capture(const QString &fileName)
{
    QFileInfo fileInfo(fileName);
    QDir dir;
    dir.mkpath(fileInfo.absolutePath());

    if (fileName.toLower().endsWith(".pdf")) {
        QPrinter printer;
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        m_page.mainFrame()->print(&printer);
        return true;
    }

    QSize viewportSize = m_page.viewportSize();
    QSize pageSize = m_page.mainFrame()->contentsSize();
    if (pageSize.isEmpty())
        return NULL;
    QImage buffer = renderToImage(pageSize);

    m_page.setViewportSize(viewportSize);
    return buffer.save(fileName);
}

QImage Chimera::renderToImage(QSize pageSize)
{
    QRect frameRect = QRect(QPoint(0,0), pageSize);
    if(!m_clipRect.isNull()){
        frameRect = m_clipRect;
        //std::cout << "Frame Clipped to rect topLeft: " << m_clipRect.topLeft().x() << "," << m_clipRect.topLeft().y() << std::endl;
        //std::cout << "Frame Clipped to rect bottomRight: " << m_clipRect.bottomRight().x() << "," << m_clipRect.bottomRight().y() << std::endl;
        pageSize = QSize(frameRect.width(),frameRect.height());
    }

#ifdef Q_OS_WIN32
    QImage::Format format = QImage::Format_ARGB32_Premultiplied;
#else
    QImage::Format format = QImage::Format_ARGB32;
#endif

    QImage buffer(pageSize, format);
    buffer.fill(Qt::transparent);

    // Render code ported from PhantomJS http://phantomjs.org/

    QPainter painter;
    // We use tiling approach to work-around Qt software rasterizer bug
    // when dealing with very large paint device.
    // See http://code.google.com/p/phantomjs/issues/detail?id=54.
    const int tileSize = 4096;
    int htiles = (buffer.width() + tileSize - 1) / tileSize;
    int vtiles = (buffer.height() + tileSize - 1) / tileSize;
    for (int x = 0; x < htiles; ++x) {
        for (int y = 0; y < vtiles; ++y) {

            QImage tileBuffer(tileSize, tileSize, format);
            tileBuffer.fill(qRgba(255, 255, 255, 0));

            // Render the web page onto the small tile first
            painter.begin(&tileBuffer);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::TextAntialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.translate(-frameRect.left(), -frameRect.top());
            painter.translate(-x * tileSize, -y * tileSize);
            m_page.mainFrame()->render(&painter, QRegion(frameRect));
            painter.end();

            // Copy the tile to the main buffer
            painter.begin(&buffer);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.drawImage(x * tileSize, y * tileSize, tileBuffer);
            painter.end();
        }
    }
    return buffer;
}


QByteArray Chimera::captureBytes()
{
    QSize viewportSize = m_page.viewportSize();
    QSize pageSize = m_page.mainFrame()->contentsSize();
    if (pageSize.isEmpty())
        return NULL;
    QImage buffer = renderToImage(pageSize);

    m_page.setViewportSize(viewportSize);

    QByteArray ba;
    QBuffer buff(&ba);
    buff.open(QIODevice::WriteOnly);
    buffer.save(&buff, "PNG");
    return buff.buffer();
}


void Chimera::clipToElement(const QString &selector){
    if(!selector.isNull()){
        QWebElement element = m_page.mainFrame()->findFirstElement(selector);
        if(!element.isNull()){
            m_clipRect = element.geometry();
            return;
        }else{
            std::cout << "Element not found for clipping" << std::endl;
        }
    }else{
        std::cout << "Selector was null for clipping" << std::endl;
    }
    // Set null rectangle
    m_clipRect.setWidth(0);
    m_clipRect.setHeight(0);
}

int Chimera::returnValue() const
{
    return m_returnValue;
}

QString Chimera::getResult()
{
    return m_results.dequeue();
}

QString Chimera::getError()
{
    return m_errors.dequeue();
}

// void Chimera::sleep(int ms)
// {
//     QTime startTime = QTime::currentTime();
//     while (true) {
//         QApplication::processEvents(QEventLoop::AllEvents, 25);
//         if (startTime.msecsTo(QTime::currentTime()) > ms)
//             break;
//     }
// }

void Chimera::setState(const QString &value)
{
    m_state = value;
}

QString Chimera::state() const
{
    return m_state;
}

void Chimera::setUserAgent(const QString &ua)
{
    m_page.m_userAgent = ua;
}

QString Chimera::userAgent() const
{
    return m_page.m_userAgent;
}

void Chimera::setViewportSize(const QVariantMap &size)
{
    int w = size.value("width").toInt();
    int h = size.value("height").toInt();
    if (w > 0 && h > 0)
        m_page.setViewportSize(QSize(w, h));
}




QVariantMap Chimera::viewportSize() const
{
    QVariantMap result;
    QSize size = m_page.viewportSize();
    result["width"] = size.width();
    result["height"] = size.height();
    return result;
}
