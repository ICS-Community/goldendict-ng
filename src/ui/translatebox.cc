/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "translatebox.hh"

#include <QAction>
#include <QHBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QModelIndex>
#include <QScrollBar>
#include <QStyle>

namespace
{
#define MAX_POPUP_ROWS 17
}

CompletionList::CompletionList(TranslateBox * parent) : WordList(parent),
  translateBox(parent)
{
#ifdef Q_OS_WIN
  setWindowFlags(Qt::ToolTip);
#else
  setParent( parent->window() );
  setAutoFillBackground( true );
#endif

  connect( this, &QAbstractItemView::activated, this, &CompletionList::acceptCurrentEntry );

  connect( this, &QListWidget::itemClicked, this, &CompletionList::acceptCurrentEntry );

  translateBox->window()->installEventFilter(this);
}

bool CompletionList::eventFilter( QObject * obj, QEvent * ev )
{
  // when the main window is moved or resized, hide the word list suggestions
  if ( obj != this && !isAncestorOf( qobject_cast< QWidget * >( obj ) )
       && ( ev->type() == QEvent::Move || ev->type() == QEvent::Resize ) )
  {
    translateBox->setPopupEnabled( false );
    return false;
  }

  return QWidget::eventFilter( obj, ev );
}

int CompletionList::preferredHeight() const
{
#if( QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ) )
  const QSize itemSizeHint = itemDelegate()->sizeHint( viewOptions(), model()->index( 0, 0 ) );
#else
  QStyleOptionViewItem option;
  initViewItemOption( &option );
  const QSize itemSizeHint = itemDelegate()->sizeHint(option, model()->index( 0, 0 ) );
#endif
  int rows = qMin( count(), MAX_POPUP_ROWS );

  int scrollBarHeight = 0;

  bool hBarIsVisible = horizontalScrollBar()->maximum() > 0;

  if ( hBarIsVisible )
    scrollBarHeight += QApplication::style()->pixelMetric( QStyle::PM_ScrollBarExtent );

  return rows == 0 ? 0 : itemSizeHint.height() * rows + frameWidth() * 2 + scrollBarHeight;
}

bool CompletionList::acceptCurrentEntry()
{
  if (!isVisible())
  {
    return false;
  }

  const QModelIndex index = currentIndex();
  if ( !index.isValid() )
  {
    return false;
  }

  emit doubleClicked(index);
  translateBox->setPopupEnabled( false );

  return true;
}

TranslateBox::TranslateBox( QWidget * parent ):
    QWidget( parent ),
    word_list( new CompletionList( this ) ),
    translate_line( new QLineEdit( this ) ),
    m_popupEnabled( false )
{
  // initially hidden
  word_list->hide();

  resize(200, 90);
  QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  setSizePolicy(sizePolicy);
  // setMinimumSize(QSize(800, 0));

  setFocusProxy(translate_line);
  translate_line->setObjectName("translateLine");
  translate_line->setPlaceholderText( tr( "Type a word or phrase to search dictionaries" ) );
  word_list->setTranslateLine(translate_line);

  // completer = new QCompleter(m_completionList->model(), this);
  // completer->setCaseSensitivity(Qt::CaseInsensitive);
  // completer->setCompletionMode(QCompleter::InlineCompletion);

  QHBoxLayout *layout = new QHBoxLayout(this);
  setLayout(layout);
  layout->setContentsMargins(0,0,0,0);
  layout->addWidget(translate_line);

  QAction * dropdown = new QAction( QIcon(":/icons/1downarrow.svg"), tr("Drop-down"),this);
  connect( dropdown,&QAction::triggered,this, &TranslateBox::rightButtonClicked );

  translate_line->addAction( dropdown,QLineEdit::TrailingPosition);
  translate_line->addAction( new QAction(QIcon(":/icons/system-search.svg"),"",this),QLineEdit::LeadingPosition);

  translate_line->setFocusPolicy(Qt::ClickFocus);

  translate_line->installEventFilter( this );
  this->installEventFilter( this );

  connect( translate_line, &QLineEdit::textChanged, this, &TranslateBox::onTextEdit );


  connect( word_list, &WordList::contentChanged, this, &TranslateBox::showPopup );
}

bool TranslateBox::eventFilter(QObject *obj, QEvent *event)
{
    // hide the suggestions list when the window is not active
    if ( event->type() == QEvent::WindowDeactivate )
    {
      if (!word_list->isActiveWindow())
      {
        setPopupEnabled( false );
      }
      return false;
    }

    if (obj == translate_line && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
          if ( !word_list->isVisible() )
          {
            setPopupEnabled( true );
          }
          else
          {
            QApplication::sendEvent(word_list, event);
          }
          return true;
        case Qt::Key_Enter:
        case Qt::Key_Return:
          return word_list->acceptCurrentEntry();
        case Qt::Key_Escape:
          setPopupEnabled( false );
          return true;
        case Qt::Key_Tab:
          if ( !word_list->isVisible() )
          {
            setPopupEnabled( true );
          }
          else
          {
            QKeyEvent event( QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier );
            QApplication::sendEvent( word_list, &event );
          }
          return true;
        case Qt::Key_Backtab:
          if ( !word_list->isVisible() )
          {
            setPopupEnabled( true );
          }
          else
          {
            QKeyEvent event( QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier );
            QApplication::sendEvent( word_list, &event );
          }
          return true;
        default:
            break;
        }
    } else if (obj == translate_line && event->type() == QEvent::FocusOut) {
#if defined(Q_OS_WIN)
        QFocusEvent *fev = static_cast<QFocusEvent*>(event);
        if (fev->reason() != Qt::ActiveWindowFocusReason ||
            (fev->reason() == Qt::ActiveWindowFocusReason && !word_list->isActiveWindow()))
#endif
          setPopupEnabled( false );
    } else if (obj == translate_line && event->type() == QEvent::FocusIn) {
      // By default, focusing the traslate line does not show
      // the popup window.
    } else if (obj == this && event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape && !ke->modifiers() && word_list->isVisible() ) {
            event->accept();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TranslateBox::setText( QString text, bool showPopup )
{
  setPopupEnabled( showPopup );
  translate_line->setText( text );
}

void TranslateBox::setPopupEnabled( bool enable )
{
  m_popupEnabled = enable;
  showPopup();
}

void TranslateBox::setSizePolicy( QSizePolicy policy )
{
  QWidget::setSizePolicy( policy );
  if ( translate_line )
    translate_line->setSizePolicy( policy );
}

void TranslateBox::showPopup()
{
  // completer->setCompletionPrefix( m_fileLineEdit->text() );
  // qDebug() << "COMPLETION:" << completer->currentCompletion();

  // Don't allow recursive call
  if( translateBoxMutex.tryLock() )
    translateBoxMutex.unlock();
  else
    return;
  QMutexLocker _( &translateBoxMutex );

  if (translate_line->text().trimmed().isEmpty() || word_list->count() == 0)
  {
    // nothing to show
    if (word_list->isVisible())
    {
      word_list->hide();
      translate_line->setFocus();
    }
    return;
  }

  if ( !m_popupEnabled )
  {
    word_list->hide();
    return;
  }

  int preferredHeight = word_list->preferredHeight();

  QPoint origin( translate_line->x(), translate_line->y() + translate_line->height() );

  if ( word_list->isWindow() )
  {
    origin = mapToGlobal( origin );
  }
  else
  {
    origin = mapTo( window(), origin );
    preferredHeight = qMin( translate_line->window()->height() - origin.y(), preferredHeight );
  }

  const QSize size(width(), preferredHeight);
  const QRect rect( origin, size );

  word_list->setGeometry(rect);
  word_list->show();
  word_list->raise();
  translate_line->setFocus();
}

QLineEdit * TranslateBox::translateLine()
{
  return translate_line;
}

WordList * TranslateBox::wordList()
{
  return word_list;
}

void TranslateBox::rightButtonClicked()
{
  setPopupEnabled( !m_popupEnabled );
}

void TranslateBox::onTextEdit()
{
  if ( translate_line->hasFocus() )
    setPopupEnabled( true );
}
