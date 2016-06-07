/**
Released as open source by Gabriel Caudrelier

Developed by Gabriel Caudrelier, gabriel dot caudrelier at gmail dot com

https://github.com/metrodango/pip3line

Released under AGPL see LICENSE for more information
**/

#include "transformwidget.h"
#include "ui_transformwidget.h"
#include "newbytedialog.h"
#include <transformabstract.h>
#include <transformmgmt.h>
#include <QComboBox>
#include <QTextStream>
#include <QSpacerItem>
#include <QMenu>
#include <QClipboard>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QTextCursor>
#include <QTableWidgetSelectionRange>
#include <QtAlgorithms>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QRgb>
#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QElapsedTimer>
#include <QMimeData>
#include <QValidator>
#include <QFileDialog>
#include <QTextCodec>
#include <threadedprocessor.h>
#include "sources/basicsource.h"
#include "shared/offsetgotowidget.h"
#include "shared/searchwidget.h"
#include "views/textview.h"
#include "views/hexview.h"
#include "shared/messagepanelwidget.h"
#include <QDebug>
#include <QRegExp>
#include "shared/clearallmarkingsbutton.h"
#include "shared/guiconst.h"
#include "views/bytetableview.h"
#include <QScrollBar>

const int TransformWidget::MAX_DIRECTION_TEXT = 20;
const QString TransformWidget::NEW_BYTE_ACTION = "New Byte(s)";

TransformWidget::TransformWidget(GuiHelper *nguiHelper ,QWidget *parent) :
    QWidget(parent)
{
    ui = new(std::nothrow) Ui::TransformWidget();
    if (ui == nullptr) {
        qFatal("Cannot allocate memory for Ui::TransformWidget X{");
    }
    currentTransform = nullptr;
    infoDialog = nullptr;
    settingsTab = nullptr;
    gotoWidget = nullptr;
    searchWidget = nullptr;
    folded = false;
    guiHelper = nguiHelper;
    transformFactory = guiHelper->getTransformFactory();
    manager = guiHelper->getNetworkManager();
    logger = guiHelper->getLogger();
    byteSource = new(std::nothrow) BasicSource();
    if (byteSource == nullptr) {
        qFatal("Cannot allocate memory for byteSource X{");
    }

    connect(byteSource, SIGNAL(log(QString,QString,Pip3lineConst::LOGLEVEL)), logger, SLOT(logMessage(QString,QString,Pip3lineConst::LOGLEVEL)), Qt::QueuedConnection);
    connect(byteSource, SIGNAL(updated(quintptr)), this, SLOT(refreshOutput()));
    connect(byteSource, SIGNAL(nameChanged(QString)), this, SIGNAL(tryNewName(QString)));

    ui->setupUi(this);

    buildSelectionArea();
    // cannot do that in the buildSelectionArea() function as it is called frequently
    ui->wayGroupBox->setVisible(false);
    ui->deleteButton->setEnabled(false);
    ui->infoPushButton->setEnabled(false);
    connect(ui->transfoComboBox, SIGNAL(currentIndexChanged(QString)), this, SLOT(onTransformSelected(QString)), Qt::UniqueConnection);

    ui->activityStackedWidget->setCurrentIndex(1);
    ui->transfoComboBox->setFocusPolicy( Qt::StrongFocus );
    ui->transfoComboBox->installEventFilter(guiHelper) ;

    configureViewArea();

    messagePanel = new(std::nothrow) MessagePanelWidget(this);
    if (messagePanel == nullptr) {
        qFatal("Cannot allocate memory for MessagePanelWidget X{");
    }


    ui->mainLayout->insertWidget(0,messagePanel);
    connect(byteSource, SIGNAL(log(QString,QString,Pip3lineConst::LOGLEVEL)), messagePanel, SLOT(log(QString,QString,Pip3lineConst::LOGLEVEL)));

    firstView = true;
    setAcceptDrops(true);

    connect(transformFactory, SIGNAL(transformsUpdated()),this, SLOT(buildSelectionArea()), Qt::QueuedConnection);
    connect(guiHelper, SIGNAL(filterChanged()), this, SLOT(buildSelectionArea()));

    connect(this, SIGNAL(sendRequest(TransformRequest*)), guiHelper->getCentralTransProc(), SLOT(processRequest(TransformRequest*)), Qt::QueuedConnection);
    connect(ui->deleteButton, SIGNAL(clicked()), SLOT(deleteMe()));

    connect(ui->foldPushButton, SIGNAL(clicked()), this, SLOT(onFoldRequest()));
    connect(ui->insertPushButton, SIGNAL(clicked(bool)), this, SIGNAL(insertRequest()));

 //   qDebug() << "Created" << this;
}

TransformWidget::~TransformWidget()
{

  //  qDebug() << "Destroying:" << this << " " << (currentTransform == nullptr ? "Null" : currentTransform->name());

    // prevent loops when destroying
    disconnect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(onCurrentTabChanged(int)));

    clearCurrentTransform();
    delete gotoWidget;
    gotoWidget = nullptr;
    delete searchWidget;
    searchWidget = nullptr;
    delete infoDialog;
    infoDialog = nullptr;
    delete hexView;
    hexView = nullptr;
    delete byteSource;
    logger = nullptr;
    guiHelper = nullptr;

    delete ui;

}

void TransformWidget::configureViewArea() {
    hexView = new(std::nothrow) HexView(byteSource,guiHelper,this);
    if (hexView == nullptr) {
        qFatal("Cannot allocate memory for HexView X{");
    }

    textView = new(std::nothrow) TextView(byteSource,guiHelper,this);
    if (textView == nullptr) {
        qFatal("Cannot allocate memory for TextView X{");
    }

    ui->tabWidget->addTab(textView,"Text");
    ui->tabWidget->addTab(hexView,"Hexadecimal");

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(onCurrentTabChanged(int)));

    hexView->installEventFilter(this);
    textView->installEventFilter(this);
    ui->tabWidget->installEventFilter(this);
    this->installEventFilter(this);

    searchWidget = new(std::nothrow) SearchWidget(byteSource,guiHelper, this);
    if (searchWidget == nullptr) {
        qFatal("Cannot allocate memory for SearchWidget X{");
    }
    searchWidget->setViewIsText(ui->tabWidget->currentWidget() == textView);
    searchWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    searchWidget->setStopVisible(false);
    ui->toolsLayout->addWidget(searchWidget);
    connect(searchWidget, SIGNAL(searchRequest(QByteArray,QBitArray,bool)), SLOT(onSearch(QByteArray,QBitArray,bool)));
    connect(textView, SIGNAL(searchStatus(bool)), searchWidget,SLOT(setError(bool)));
    connect(searchWidget, SIGNAL(jumpTo(quint64,quint64)), hexView, SLOT(gotoSearch(quint64,quint64)));

    gotoWidget = new(std::nothrow) OffsetGotoWidget(guiHelper, this);
    if (gotoWidget == nullptr) {
        qFatal("Cannot allocate memory for OffsetGotoWidget X{");
    }

    gotoWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->toolsLayout->addWidget(gotoWidget);
    connect(gotoWidget, SIGNAL(gotoRequest(quint64,bool,bool,bool)), SLOT(onGotoOffset(quint64,bool,bool,bool)));

    clearAllMarkingsButton = new(std::nothrow) ClearAllMarkingsButton(byteSource,this);
    if (clearAllMarkingsButton == nullptr) {
        qFatal("Cannot allocate memory for ClearAllMarkingsButton X{");
    }
    ui->uppperToolLayout->insertWidget(1,clearAllMarkingsButton);

    connect(ui->backwardPushButton, SIGNAL(clicked()), this, SLOT(onHistoryBackward()));
    connect(ui->forwardPushButton, SIGNAL(clicked()), this, SLOT(onHistoryForward()));

    connect(textView, SIGNAL(invalidText()), this, SLOT(onInvalidText()));
    connect(textView, SIGNAL(askForFileLoad()), this, SLOT(onFileLoadRequest()));
    connect(hexView, SIGNAL(askForFileLoad()), this, SLOT(onFileLoadRequest()));
}

void TransformWidget::buildSelectionArea() {
    QString currentTransformName;

    if (ui->transfoComboBox->currentIndex() != -1) {
        currentTransformName = ui->transfoComboBox->currentText();
    }
    //cleaning

    ui->transfoComboBox->blockSignals(true);
    ui->transfoComboBox->clear();

    guiHelper->buildTransformComboBox(ui->transfoComboBox, currentTransformName, true);
    ui->transfoComboBox->blockSignals(false);
}

void TransformWidget::clearCurrentTransform()
{
    if (currentTransform != nullptr) {
        delete settingsTab;
        settingsTab = nullptr;
        delete currentTransform;
        currentTransform = nullptr;
        if (ui->tabWidget->count() > 2)
            ui->tabWidget->removeTab(2);
    }

    delete infoDialog;
    infoDialog = nullptr;
}

void TransformWidget::updatingFromTransform() {
    configureDirectionBox();
    emit transformChanged();
    refreshOutput();
}

void TransformWidget::onHistoryBackward()
{
    byteSource->historyBackward();
}

void TransformWidget::onHistoryForward()
{
    byteSource->historyForward();
}

void TransformWidget::onTransformSelected(QString name) {
    clearCurrentTransform();

    currentTransform = transformFactory->getTransform(name);
    if (currentTransform != nullptr) {
        currentTransform->setWay(TransformAbstract::INBOUND);
        integrateTransform();
        buildSelectionArea();
        emit transfoRequest();
    } else {
        QString mess = tr("Could not instantiate transformation :%1").arg(name);
        logger->logError(mess);
        QMessageBox::critical(this, tr("Error"), mess,QMessageBox::Ok);
    }
}

void TransformWidget::integrateTransform()
{
    if (currentTransform != nullptr) {
        ui->descriptionLabel->setText(currentTransform->description());
        // need to grab the errors before getting the gui, in case the gui configuration generates some
        connect(currentTransform, SIGNAL(error(QString,QString)), this, SLOT(logError(QString)));
        connect(currentTransform, SIGNAL(warning(QString,QString)), this, SLOT(logWarning(QString)));

        settingsTab = currentTransform->getGui(this);
        if (settingsTab != nullptr) {
            ui->tabWidget->addTab(settingsTab, tr("Settings"));
        }

        configureDirectionBox();
        connect(currentTransform,SIGNAL(confUpdated()),this,SLOT(updatingFromTransform()), Qt::QueuedConnection);
        ui->deleteButton->setEnabled(true);
        ui->infoPushButton->setEnabled(true);
        emit transformChanged();
        refreshOutput();
    }
}

void TransformWidget::input(QByteArray inputdata) {
    byteSource->setData(inputdata);
    if (firstView) {
        if (byteSource->isReadableText()) {
            ui->tabWidget->setCurrentWidget(textView);
        }
        else {
            ui->tabWidget->setCurrentWidget(hexView);
        }
        firstView = false;
    }
}

QByteArray TransformWidget::output() {
    return outputData;
}

void TransformWidget::refreshOutput()
{
    if (currentTransform != nullptr) {
        ui->activityStackedWidget->setCurrentIndex(0);

        TransformAbstract * ta = transformFactory->cloneTransform(currentTransform);
        if (ta != nullptr) {
            TransformRequest *tr = new(std::nothrow) TransformRequest(
                        ta,
                        byteSource->getRawData(),
                        (quintptr) this);

            if (tr == nullptr) {
                qFatal("Cannot allocate memory for TransformRequest X{");
            }

            connect(tr,SIGNAL(finishedProcessing(QByteArray,Messages)), this, SLOT(processingFinished(QByteArray,Messages)));
            emit sendRequest(tr);
        }
    } else { // if no transform just publish the input data
        outputData = byteSource->getRawData();
        emit updated();
    }
}

void TransformWidget::processingFinished(QByteArray output, Messages messages)
{
    messagePanel->clear();
    outputData = output;

    if (messages.isEmpty()) {
        messagePanel->closeWidget();
    } else {
        for (int i = 0; i < messages.size() ; i++) {
            switch (messages.at(i).level) {
                case (LERROR):
                    logError(messages.at(i).message, messages.at(i).source);
                    break;
                case (LWARNING):
                    logWarning(messages.at(i).message, messages.at(i).source);
                    break;
                case (LSTATUS):
                    logStatus(messages.at(i).message, messages.at(i).source);
                    break;
                default:
                    qWarning("[TransformWidget::processingFinished] Unkown error level");
            }
        }
    }

    emit updated();
    ui->activityStackedWidget->setCurrentIndex(1);
}

void TransformWidget::on_encodeRadioButton_toggled(bool checked)
{
    if (checked && currentTransform != nullptr) {
        currentTransform->setWay(TransformAbstract::INBOUND);
    }
}

void TransformWidget::on_decodeRadioButton_toggled(bool checked)
{
    if (checked && currentTransform != nullptr) {
        currentTransform->setWay(TransformAbstract::OUTBOUND);
    }
}

void TransformWidget::updatingFrom() {
    TransformWidget* src = dynamic_cast<TransformWidget*>(sender());
    input(src->output());
}

TransformAbstract *TransformWidget::getTransform() {
    return currentTransform;
}

bool TransformWidget::setTransform(TransformAbstract * transf)  {
    if (transf != nullptr) {
        clearCurrentTransform();

        currentTransform = transf;

        ui->transfoComboBox->blockSignals(true);
        ui->transfoComboBox->setCurrentIndex(ui->transfoComboBox->findText(currentTransform->name()));
        ui->transfoComboBox->blockSignals(false);

        integrateTransform();
        return true;
    }
    return false;
}

ByteSourceAbstract *TransformWidget::getSource()
{
    return byteSource;
}

ByteTableView *TransformWidget::getHexTableView()
{
    return hexView->getHexTableView();
}

void TransformWidget::copyTextToClipboard()
{
    textView->copyToClipboard();
}

BaseStateAbstract *TransformWidget::getStateMngtObj()
{
    TransformWidgetStateObj *stateObj = new(std::nothrow) TransformWidgetStateObj(this);
    if (stateObj == nullptr) {
        qFatal("Cannot allocate memory for TransformWidgetStateObj X{");
    }

    return stateObj;
}

QString TransformWidget::getDescription()
{
    QString ret(GuiConst::NO_TRANSFORM);
    if (currentTransform != nullptr) {
        ret = (currentTransform->name());
        if (currentTransform->isTwoWays()) {
            ret.append(QString(" [%2]")
                       .arg(currentTransform->way() == TransformAbstract::INBOUND ?
                               currentTransform->inboundString() :
                               currentTransform->outboundString() ));
        }
    }

    return ret;
}

void TransformWidget::forceUpdating()
{
    refreshOutput();
}

void TransformWidget::logWarning(const QString message, const QString source) {
    messagePanel->addMessage(message,Qt::blue);
    emit warning(message, source);
}

void TransformWidget::logError(const QString message, const QString source) {
    messagePanel->addMessage(message,Qt::red);
    emit error(message,source);
}

void TransformWidget::logStatus(const QString message, const QString source)
{
    messagePanel->addMessage(message,Qt::black);
    emit status(message,source);
}

void TransformWidget::reset()
{
    if (currentTransform != nullptr) {
        clearCurrentTransform();

        ui->transfoComboBox->blockSignals(true);
        ui->transfoComboBox->setCurrentIndex(0);
        ui->transfoComboBox->blockSignals(false);
        ui->wayGroupBox->setVisible(false);
        delete settingsTab;
        settingsTab = nullptr;
        ui->descriptionLabel->clear();
    }
    byteSource->clear();
    ui->deleteButton->setEnabled(false);
    ui->infoPushButton->setEnabled(false);
    ui->tabWidget->setCurrentWidget(textView);
    if (folded) {
        emit resetDone();
    }
}

void TransformWidget::configureDirectionBox()
{
    if (currentTransform->isTwoWays()) {
        if (currentTransform->way() == TransformAbstract::INBOUND) {
            ui->encodeRadioButton->setChecked(true);
        } else {
            ui->decodeRadioButton->setChecked(true);
        }
        QString tempText = currentTransform->inboundString();
        if (tempText.size() > MAX_DIRECTION_TEXT) {
            tempText = tempText.mid(0,MAX_DIRECTION_TEXT);
        }
        ui->encodeRadioButton->setText(tempText);
        tempText = currentTransform->outboundString();
        if (tempText.size() > MAX_DIRECTION_TEXT) {
            tempText = tempText.mid(0,MAX_DIRECTION_TEXT);
        }
        ui->decodeRadioButton->setText(tempText);

        ui->wayGroupBox->setEnabled(true);
        ui->wayGroupBox->setVisible(true);
    } else {
        ui->wayGroupBox->setEnabled(false);
        ui->wayGroupBox->setVisible(false);
    }
}

bool TransformWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_N && keyEvent->modifiers().testFlag(Qt::ControlModifier))  {
            if (ui->tabWidget->currentWidget() == textView) {
                textView->searchAgain();
            } else {
                searchWidget->nextFind(hexView->getLowPos());
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_G && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            gotoWidget->setFocus();
            return true;
        } else if (keyEvent->key() == Qt::Key_F && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            searchWidget->setFocus();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void TransformWidget::updateView(quintptr)
{
    refreshOutput();
}

void TransformWidget::onInvalidText()
{
    ui->tabWidget->setCurrentWidget(hexView);
}

void TransformWidget::dragEnterEvent(QDragEnterEvent *event)
{
    guiHelper->processDragEnter(event, byteSource);
}

void TransformWidget::dropEvent(QDropEvent *event)
{
    guiHelper->processDropEvent(event, byteSource);
}

bool TransformWidget::isFolded() const
{
    return folded;
}

void TransformWidget::setFolded(bool value)
{
    folded = value;
}

void TransformWidget::enableDeletetion(bool enabled)
{
    ui->deleteButton->setEnabled(enabled);
}

void TransformWidget::fromLocalFile(QString fileName)
{
    byteSource->fromLocalFile(fileName);
}

void TransformWidget::onFileLoadRequest()
{
    QString fileName;
    fileName = QFileDialog::getOpenFileName(this,tr("Choose file to load from"));
    if (!fileName.isEmpty()) {
        fromLocalFile(fileName);
    }
}

void TransformWidget::deleteMe()
{
    emit deletionRequest();
}

void TransformWidget::setAutoCopyTextToClipboard(bool val)
{
    textView->setAutoCopyToClipboard(val);
}

void TransformWidget::on_infoPushButton_clicked()
{
    if (currentTransform == nullptr)
        return;

    if (infoDialog == nullptr) {
        infoDialog = new(std::nothrow) InfoDialog(guiHelper, currentTransform,this);
        if (infoDialog == nullptr) {
            qFatal("Cannot allocate memory for InfoDialog X{");
            return;
        }
    }
    infoDialog->setVisible(true);
}

void TransformWidget::on_clearDataPushButton_clicked()
{
    byteSource->clear();
}

void TransformWidget::onSearch(QByteArray item, QBitArray mask, bool couldBeText)
{
    if (ui->tabWidget->currentWidget() == textView && couldBeText) {
        textView->search(searchWidget->text().toUtf8());
    } else {
        ui->tabWidget->setCurrentWidget(hexView);
        hexView->search(item, mask);
    }
}

void TransformWidget::onGotoOffset(quint64 offset, bool absolute, bool negative, bool select)
{
    if (!hexView->goTo(offset,absolute,negative, select)) {
        gotoWidget->setStyleSheet(GuiStyles::LineEditError);
    } else {
        ui->tabWidget->setCurrentWidget(hexView);
        gotoWidget->setStyleSheet(qApp->styleSheet());
    }
}

void TransformWidget::onFoldRequest()
{
    emit foldRequest();
}

void TransformWidget::onCurrentTabChanged(int index)
{
    if (ui->tabWidget->widget(index) == textView) {
        searchWidget->setViewIsText(true);
    } else {
       searchWidget->setViewIsText(false);
    }
}


TransformWidgetStateObj::TransformWidgetStateObj(TransformWidget *tw) :
    tw(tw)
{
    name = tw->metaObject()->className();
}

TransformWidgetStateObj::~TransformWidgetStateObj()
{

}

void TransformWidgetStateObj::run()
{
    if (flags & GuiConst::STATE_SAVE_REQUEST) {
        writer->writeStartElement(tw->metaObject()->className());
        writer->writeAttribute(GuiConst::STATE_CURRENT_INDEX, write(tw->ui->tabWidget->currentIndex()));
        writer->writeAttribute(GuiConst::STATE_SCROLL_INDEX, write(tw->hexView->getHexTableView()->verticalScrollBar()->value()));
        writer->writeAttribute(GuiConst::STATE_IS_FOLDED, write(tw->isFolded()));
        writer->writeEndElement();
    } else {
        bool gotIt = false;
        //need to check if we are not already on the token due to previous checks
        if (reader->tokenType() == QXmlStreamReader::StartElement && reader->name() == tw->metaObject()->className()) {
            gotIt = true;
        } else if (readNextStart(tw->metaObject()->className())) {
            gotIt = true;
        }

        if (gotIt) {
            QXmlStreamAttributes attrList = reader->attributes();
            bool ok = false;
            int index = 0;
            if (attrList.hasAttribute(GuiConst::STATE_CURRENT_INDEX)) {
                index = readInt(attrList.value(GuiConst::STATE_CURRENT_INDEX), &ok);
                if (ok && index < tw->ui->tabWidget->count()) {
                    tw->ui->tabWidget->setCurrentIndex(index);
                }
            }

            if (attrList.hasAttribute(GuiConst::STATE_SCROLL_INDEX)) {
                index = readInt(attrList.value(GuiConst::STATE_SCROLL_INDEX), &ok);
                if (ok) {
                    tw->hexView->getHexTableView()->verticalScrollBar()->setValue(index);
                }
            }

            if (attrList.hasAttribute(GuiConst::STATE_IS_FOLDED)) {
                ok = readBool(attrList.value(GuiConst::STATE_IS_FOLDED));
                if (ok) {
                    TransformWidgetFoldingObj * state = new(std::nothrow) TransformWidgetFoldingObj(tw);
                    if (state == nullptr) {
                        qFatal("Cannot allocate memory for TransformWidgetFoldingObj X{");
                    }
                    emit addNewState(state);
                }
            }

            readEndElement(tw->metaObject()->className());// reading closing tag classname
        }
    }
}


TransformWidgetFoldingObj::TransformWidgetFoldingObj(TransformWidget *tw) :
    tw(tw)
{
    name = "Folding Transform View";
}

TransformWidgetFoldingObj::~TransformWidgetFoldingObj()
{

}

void TransformWidgetFoldingObj::run()
{
    tw->onFoldRequest();
}