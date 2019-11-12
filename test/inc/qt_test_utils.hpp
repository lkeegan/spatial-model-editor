// some useful routines for testing Qt widgets

#pragma once

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QObject>
#include <QTest>
#include <QTimer>
#include <queue>

const int mouseDelay = 50;

// timer class that repeatedly checks for an active modal widget
// (a modal widget blocks execution pending user input, e.g. a message box)
// and when it finds a modal widget
//    - stores some text about it in results
//    - optionally start another ModalWidgetTimer
//       - for the case where the UserAction will open another modal widget
//       - the new modal widget blocks execution of the currently running
//       ModalWidgetTimer
//       - so the newly started one can deal with & close the new modal widget
//       - once closed, execution returns to the current ModalWidgetTimer
//    - sends KeyEvents for the keys specified in UserAction to the modal widget
//    - optionally calls accept (i.e. press OK) on the modal widget
// default timing:
//    - check for a modal widget every 0.1s
//    - give up after 30s if no modal widget found

class ModalWidgetTimer : public QObject {
  Q_OBJECT

  using KeyPair = std::pair<QKeySequence, QChar>;
  struct UserAction {
    std::vector<KeyPair> keySequence;
    QString message;
    bool callAccept = true;
    ModalWidgetTimer *mwtToStart = nullptr;
  };

 private:
  int timeLeft;
  QTimer timer;
  QStringList results;
  std::queue<UserAction> userActions;

  void getText(QWidget *widget) {
    // check if widget is a QFileDialog
    QString result;
    auto *p = qobject_cast<QFileDialog *>(widget);
    if (p != nullptr) {
      if (p->acceptMode() == QFileDialog::AcceptOpen) {
        result = "QFileDialog::AcceptOpen";
      } else if (p->acceptMode() == QFileDialog::AcceptSave) {
        result = "QFileDialog::AcceptSave";
      }
    }
    // check if widget is a QMessageBox
    auto *msgBox = qobject_cast<QMessageBox *>(widget);
    if (msgBox != nullptr) {
      result = msgBox->text();
    }
    results.push_back(result);
  }

  void sendKeyEvents(QWidget *widget) {
    const auto &action = userActions.front();
    if (action.mwtToStart != nullptr) {
      qDebug() << this << ":: starting" << action.mwtToStart;
      action.mwtToStart->start();
    }
    qDebug() << this << ":: entering" << action.message << "into" << widget;
    for (const auto &[keySeq, character] : action.keySequence) {
      auto press =
          new QKeyEvent(QEvent::KeyPress, keySeq[0], Qt::NoModifier, character);
      QCoreApplication::postEvent(widget->windowHandle(), press);
      QApplication::processEvents();
      auto release = new QKeyEvent(QEvent::KeyRelease, keySeq[0],
                                   Qt::NoModifier, character);
      QCoreApplication::postEvent(widget->windowHandle(), release);
      QApplication::processEvents();
    }
    if (action.callAccept) {
      auto *p = qobject_cast<QDialog *>(widget);
      if (p != nullptr) {
        qDebug() << this << ":: calling accept on widget" << widget;
        QApplication::processEvents();
        p->accept();
      }
    }
  }

  void lookForWidget() {
    QWidget *widget = QApplication::activeModalWidget();
    if (widget) {
      qDebug() << this << ":: found widget" << widget;
      getText(widget);
      sendKeyEvents(widget);
      userActions.pop();
      qDebug() << this
               << ":: action done, remaining actions:" << userActions.size();
      if (userActions.empty()) {
        timer.stop();
      }
    }
    timeLeft -= timer.interval();
    if (timeLeft < 0) {
      // give up
      qDebug() << this << ":: timeout: no ModalWidget found";
      timer.stop();
    }
  }

 public:
  explicit ModalWidgetTimer(int timerInterval = 100, int timeout = 30000)
      : QObject(nullptr), timeLeft(timeout) {
    timer.setInterval(timerInterval);
    QObject::connect(&timer, &QTimer::timeout, this,
                     &ModalWidgetTimer::lookForWidget);
  }

  void addUserAction(const QString &msg = {}, bool callAcceptOnDialog = true,
                     ModalWidgetTimer *otherMwtToStart = nullptr) {
    auto &action = userActions.emplace();
    for (QChar c : msg) {
      action.keySequence.emplace_back(c, c);
    }
    action.message = msg;
    action.callAccept = callAcceptOnDialog;
    action.mwtToStart = otherMwtToStart;
  }

  void addUserAction(const QStringList &keySeqStrings = {},
                     bool callAcceptOnDialog = true,
                     ModalWidgetTimer *otherMwtToStart = nullptr) {
    auto &action = userActions.emplace();
    for (const auto &s : keySeqStrings) {
      if (s.size() == 1) {
        // string is a single char
        action.keySequence.emplace_back(s[0], s[0]);
      } else {
        // string is not a char, e.g. "Ctrl" or "Tab"
        action.keySequence.emplace_back(s, QChar());
      }
      action.message.append(QString("%1 ").arg(s));
    }
    action.callAccept = callAcceptOnDialog;
    action.mwtToStart = otherMwtToStart;
  }

  void start() {
    qDebug() << this << " :: starting timer";
    if (userActions.empty()) {
      qDebug() << this << " :: no UserActions defined: adding default";
      userActions.emplace();
    }
    results.clear();
    timer.start();
  }

  const QString &getResult(int i = 0) const { return results[i]; }

  bool isRunning() const { return timer.isActive(); }
};
