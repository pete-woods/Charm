#include <QMenu>
#include <QtDebug>
#include <QListView>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDateTimeEdit>

#include "TasksView.h"
#include "ViewHelpers.h"
#include "Data.h"
#include "Core/Event.h"
#include "Core/CharmConstants.h"
#include "Application.h"
#include "EventView.h"
#include "EventDisplay.h"
#include "EventEditor.h"
#include "Core/Configuration.h"
#include "EventEditorDelegate.h"
#include "Reports/CharmReport.h"
#include "Core/TaskTreeItem.h"
#include "Core/CharmDataModel.h"
#include "SelectTaskDialog.h"
#include "EventModelFilter.h"
#include "Commands/CommandMakeEvent.h"
#include "Commands/CommandModifyEvent.h"
#include "Commands/CommandDeleteEvent.h"

#include "ui_EventView.h"

EventView::EventView( MainWindow* parent )
    : QWidget( parent )
    , m_ui( new Ui::EventView )
    , m_eventDisplay( new EventDisplay() )
    , m_view( parent )
    , m_selectedTask( 0 )
    , m_dirty( false )
    , m_model( 0 )
    , m_actionNewEvent( this )
    , m_actionDeleteEvent( this )
    , m_actionPreviousEvent( this )
    , m_actionNextEvent( this )
{
    m_ui->setupUi( this );
    QHBoxLayout* layout = new QHBoxLayout();
    layout->setContentsMargins( 0, 0, 0, 0 );
    if( m_ui->pageEvent->layout() ) {
    	delete m_ui->pageEvent->layout();
    }
    layout->addWidget( m_eventDisplay );
    m_ui->pageEvent->setLayout( layout );
    m_ui->stackedWidget->setCurrentWidget( m_ui->pageNoEvent );

    m_ui->frame->setFrameStyle( m_ui->listView->frameStyle() );
    m_ui->listView->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( m_ui->listView,
             SIGNAL( customContextMenuRequested( const QPoint& ) ),
             SLOT( slotContextMenuRequested( const QPoint& ) ) );
    connect( m_ui->listView,
			 SIGNAL( doubleClicked( const QModelIndex& ) ),
			 SLOT( slotEventDoubleClicked( const QModelIndex& ) ) );
    connect( &m_actionNextEvent, SIGNAL( triggered() ),
             SLOT( slotNextEvent() ) );
    connect( &m_actionPreviousEvent, SIGNAL( triggered() ),
             SLOT( slotPreviousEvent() ) );
    connect( &m_actionNewEvent, SIGNAL( triggered() ),
             SLOT( slotNewEvent() ) );
    connect( &m_actionDeleteEvent, SIGNAL( triggered() ),
             SLOT( slotDeleteEvent() ) );
    connect( m_eventDisplay, SIGNAL( editEvent( Event ) ),
			 SLOT( slotEditEvent( Event ) ) );
//     connect( &m_commitTimer, SIGNAL( timeout() ),
//              SLOT( slotCommitTimeout() ) );
//     m_commitTimer.setSingleShot( true );


    m_actionNewEvent.setText( tr( "Create New Event..." ) );
    m_actionNewEvent.setIcon( Data::newTaskIcon() );
    m_ui->toolButtonNewEvent->setDefaultAction( &m_actionNewEvent );

    m_actionDeleteEvent.setText( tr( "Delete Event..." ) );
    m_actionDeleteEvent.setIcon( Data::deleteTaskIcon() );
    m_ui->toolButtonDeleteEvent->setDefaultAction( &m_actionDeleteEvent );

    m_actionPreviousEvent.setIcon( Data::previousEventIcon() );
    m_actionPreviousEvent.setToolTip( tr( "Next Event" ) );
    m_ui->toolButtonPrevious->setDefaultAction( &m_actionPreviousEvent );

    m_actionNextEvent.setIcon( Data::nextEventIcon() );
    m_actionNextEvent.setToolTip( tr( "Previous Event" ) );
    m_ui->toolButtonNext->setDefaultAction( &m_actionNextEvent );

    // disable all actions, action state will be set when the current
    // item changes:
    m_actionNextEvent.setEnabled( false );
    m_actionPreviousEvent.setEnabled( false );
    m_actionNewEvent.setEnabled( true ); // always on
    m_actionDeleteEvent.setEnabled( false );

    connect( m_ui->comboBox, SIGNAL( currentIndexChanged( int ) ),
             SLOT( timeFrameChanged( int ) ) );

    QTimer::singleShot( 0, this, SLOT( delayedInitialization() ) );
}

EventView::~EventView()
{
    delete m_ui; m_ui = 0;
}

void EventView::delayedInitialization()
{
    timeSpansChanged();
    connect( &Application::instance().timeSpans(),
             SIGNAL( timeSpansChanged() ),
             SLOT( timeSpansChanged() ) );

}

void EventView::timeSpansChanged()
{
    m_timeSpans = Application::instance().timeSpans().standardTimeSpans();
    // close enough to "ever" for our purposes:
    NamedTimeSpan allEvents = {
        tr( "Ever" ),
        TimeSpan( QDate::currentDate().addYears( -200 ),
                  QDate::currentDate().addYears( +200 ) )
    };
    m_timeSpans << allEvents;

    const int currentIndex = m_ui->comboBox->currentIndex();
    m_ui->comboBox->clear();
    for ( int i = 0; i < m_timeSpans.size(); ++i )
    {
        m_ui->comboBox->addItem( m_timeSpans[i].name );
    }
    if ( currentIndex >= 0 &&  currentIndex <= m_timeSpans.size() ) {
        m_ui->comboBox->setCurrentIndex( currentIndex );
    } else {
        m_ui->comboBox->setCurrentIndex( 0 );
    }
}

void EventView::closeEvent( QCloseEvent* e )
{
    e->setAccepted( false );
    reject();
}

void EventView::reject()
{
    // there is no reject semantic - all changes are implicit:
    commitChanges();
    emit visible( false );
}

void EventView::commitCommand( CharmCommand* command )
{
    command->finalize();
    delete command;
}

void EventView::slotEventChanged()
{
    Event event = newSettings();
    if ( m_event != event ) {
        m_dirty = true;

#if 0
        const QDateTime& start = m_ui->dateTimeStart->dateTime();
        if ( start != m_event.startDateTime() ) {
            m_ui->dateTimeEnd->setMinimumTime( start.time() );
            m_ui->dateTimeEnd->setMinimumDate( start.date() );
        }
#endif

//         m_commitTimer.start( 10000 );
    }
}

void EventView::slotCommitTimeout()
{
    if ( m_dirty ) {
        commitChanges();
    }
}

void EventView::slotCurrentItemChanged( const QModelIndex& start,
                                          const QModelIndex& end )
{
    commitChanges();

    if ( ! start.isValid() ) {
        m_ui->stackedWidget->setCurrentWidget( m_ui->pageNoEvent );
        m_event = Event();
        m_selectedTask = 0;
        m_actionDeleteEvent.setEnabled(false);
    } else {
        m_ui->stackedWidget->setCurrentWidget( m_ui->pageEvent );
        m_actionDeleteEvent.setEnabled(true);
        Event event = m_model->eventForIndex( start );
        Q_ASSERT( event.isValid() ); // index is valid,  so...
        m_eventDisplay->setEvent( event );
        setCurrentEvent( event );
    }

    slotConfigureUi();
}

// FIXME obsolete
void EventView::setCurrentEvent( const Event& event )
{
    m_event = event;

    const TaskTreeItem& taskTreeItem =
        MODEL.charmDataModel()->taskTreeItem( m_event.taskId() );
}

void EventView::slotNewEvent()
{
    SelectTaskDialog dialog( this );
    if ( dialog.exec() ) {
        const TaskTreeItem& item =
            MODEL.charmDataModel()->taskTreeItem( dialog.selectedTask() );
        if ( item.task().isValid() ) {
            CommandMakeEvent* command =
                new CommandMakeEvent( item.task(), this );
            emitCommand( command );
        }
    }
}

void EventView::slotDeleteEvent()
{
    const TaskTreeItem& taskTreeItem =
        MODEL.charmDataModel()->taskTreeItem( m_event.taskId() );
    const QString name = tasknameWithParents( taskTreeItem.task() );
    const QDate date = m_event.startDateTime().date();
    const QTime time = m_event.startDateTime().time();
    const QString dateAndDuration = date.toString( Qt::SystemLocaleDate )
           + ' ' + time.toString( Qt::SystemLocaleDate )
           + ' ' + hoursAndMinutes( m_event.duration() );
    const QString eventDescription = name + ' ' + dateAndDuration;
    if ( QMessageBox::question(
             this, tr( "Delete Event?" ),
             tr( "<html>Do you really want to delete the event <b>%1</b>?" ).arg(eventDescription),
             QMessageBox::Ok | QMessageBox::Cancel,
             QMessageBox::Ok )
         == QMessageBox::Ok ) {
        CommandDeleteEvent* command = new CommandDeleteEvent( m_event, this );
        command->prepare();
        emitCommand( command );
    }
}

void EventView::slotPreviousEvent()
{
    const QModelIndex& index = m_model->indexForEvent( m_event );
    Q_ASSERT( index.isValid() && index.row() > 0 && index.row() < m_model->rowCount() );
    const QModelIndex& previousIndex = m_model->index( index.row() - 1, 0, QModelIndex() );
    m_ui->listView->selectionModel()->setCurrentIndex
        ( previousIndex, QItemSelectionModel::ClearAndSelect );
}

void EventView::slotNextEvent()
{
    const QModelIndex& index = m_model->indexForEvent( m_event );
    Q_ASSERT( index.isValid() && index.row() >= 0 && index.row() < m_model->rowCount() - 1 );
    const QModelIndex& nextIndex = m_model->index( index.row() + 1, 0, QModelIndex() );
    m_ui->listView->selectionModel()->setCurrentIndex
        ( nextIndex, QItemSelectionModel::ClearAndSelect );
}

void EventView::slotContextMenuRequested( const QPoint& point )
{
    // prepare the menu:
    QMenu menu( m_ui->listView );
    menu.addAction( &m_actionNewEvent );
    menu.addAction( &m_actionDeleteEvent );

    // all actions are handled in their own slots:
    menu.exec( m_ui->listView->mapToGlobal( point ) );
}

// FIXME obsolete
Event EventView::newSettings()
{
    Event event( m_event );
    return event;
}

void EventView::commitChanges()
{
    if ( m_dirty && m_selectedTask != 0 ) {
        CommandModifyEvent* command =
            new CommandModifyEvent( newSettings(), this );
        m_dirty = false;
        emitCommand( command );
    }
}

// FIXME obsolete
void EventView::slotSelectTask()
{
    SelectTaskDialog dialog( this );

    if ( dialog.exec() ) {
        m_selectedTask = dialog.selectedTask();
        const TaskTreeItem& taskTreeItem =
            MODEL.charmDataModel()->taskTreeItem( m_selectedTask );
        QString name = tasknameWithParents( taskTreeItem.task() );
        // m_ui->labelTaskName->setText( name );
        m_dirty = true;
    }
}

void EventView::makeVisibleAndCurrent( const Event& event )
{
    // make sure the event filter time span includes the events start
    // time (otherwise it is not visible):
    // (how?: if the event is not in the timespan, expand the timespan
    // as much as needed)
    const int CurrentTimeSpan = m_ui->comboBox->currentIndex();

    if ( ! m_timeSpans[CurrentTimeSpan].contains( event.startDateTime().date() ) ) {
        for ( int i = 0; i < m_timeSpans.size(); ++i )
        {   // at least "ever"  should contain it
            if ( m_timeSpans[i].contains( event.startDateTime().date() ) ) {
                m_ui->comboBox->setCurrentIndex( i );
                break;
            }
        }
    }
    // get an index for the event, and make it the current index:
    const QModelIndex& index = m_model->indexForEvent( event );
    Q_ASSERT( index.isValid() );
    m_ui->listView->selectionModel()->setCurrentIndex
        ( index, QItemSelectionModel::ClearAndSelect );
}

void EventView::timeFrameChanged( int index )
{
    // wait for the next update, in this case:
    if ( m_ui->comboBox->count() == 0 ) return;
    if ( !m_model ) return;
    // this is kind-of strange - we have to clear the dirty flag,
    // and this will change the current index, triggering a commit
    // halfway during the change:
    Event event = newSettings();
    bool dirty = m_dirty;
    m_dirty = false;

    if ( index >= 0 && index < m_timeSpans.size() ) {
        m_model->setFilterStartDate( m_timeSpans[index].timespan.first );
        m_model->setFilterEndDate( m_timeSpans[index].timespan.second );
    } else {
        Q_ASSERT( false );
    }

    if ( dirty ) {
        CommandModifyEvent* command =
            new CommandModifyEvent( event, this );
        emitCommand( command );
    }
}

void EventView::slotEventActivated( EventId )
{
    slotConfigureUi();
}

void EventView::slotEventDeactivated( EventId )
{
    slotConfigureUi();
}

void EventView::slotConfigureUi()
{
    // what a fricking hack - but QDateTimeEdit does not seem to have
    // a simple function to toggle 12h and 24h mode:
    static QString OriginalDateTimeFormat;
    if ( OriginalDateTimeFormat.isEmpty() ) {
        QDateTimeEdit edit( this );
        OriginalDateTimeFormat = edit.displayFormat();
    } // yeah, I know, this will survive changes in the user prefs

    bool active = MODEL.charmDataModel()->isEventActive( m_event.id() );

    m_actionNewEvent.setEnabled( true ); // always on
    m_actionDeleteEvent.setEnabled( m_event.isValid() && ! active );
    // m_ui->frame->setEnabled( ! active );

    const QModelIndex& index = m_ui->listView->selectionModel()->currentIndex();
    if ( index.isValid() ) {
        int row = index.row();
        Q_ASSERT( row >= 0 && row < m_model->rowCount() );
        m_actionPreviousEvent.setEnabled( row > 0 && row < m_model->rowCount() );
        m_actionNextEvent.setEnabled( row >=0 && row < m_model->rowCount() -1 );
    } else {
        m_actionNextEvent.setEnabled( false );
        m_actionPreviousEvent.setEnabled( false );
    }
}

void EventView::slotUpdateCurrent()
{
    // this may be Qt-version dependant, verify later:
    Event event = DATAMODEL->eventForId( m_event.id() );
    if ( event != m_event ) {
        setCurrentEvent( event );
        m_dirty = false;
    }
    slotUpdateTotal();
}

void EventView::slotUpdateTotal()
{   // what matching signal does the proxy emit?
    int seconds = m_model->totalDuration();
    if ( seconds == 0 ) {
        m_ui->labelTotal->clear();
    } else {
        QString total;
        QTextStream stream( &total );
        stream << "(" << hoursAndMinutes( seconds ) << " total)";
        m_ui->labelTotal->setText( total );
    }
}

// ViewModeInterface:
void EventView::saveGuiState() {}
void EventView::restoreGuiState() {}
void EventView::stateChanged( State previous ) {}

void EventView::configurationChanged()
{
    slotConfigureUi();
}

void EventView::setModel( ModelConnector* connector )
{
    Q_ASSERT( m_ui );
    EventModelFilter* model = connector->eventModel();
    m_ui->listView->setModel( model );
    m_ui->listView->setSelectionBehavior( QAbstractItemView::SelectRows );
    m_ui->listView->setSelectionMode( QAbstractItemView::SingleSelection );

    connect( m_ui->listView->selectionModel(),
             SIGNAL( currentChanged( const QModelIndex&, const QModelIndex& ) ),
             SLOT( slotCurrentItemChanged( const QModelIndex&, const QModelIndex& ) ) );

    connect( model, SIGNAL( eventActivationNotice( EventId ) ),
             SLOT( slotEventActivated( EventId ) ) );
    connect( model, SIGNAL( eventDeactivationNotice( EventId ) ),
             SLOT( slotEventDeactivated( EventId ) ) );

    connect( model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
             SLOT( slotUpdateCurrent() ) );
    connect( model, SIGNAL( rowsInserted( const QModelIndex&, int, int ) ),
             SLOT( slotUpdateTotal() ) );
    connect( model, SIGNAL( rowsRemoved( const QModelIndex&, int, int ) ),
             SLOT( slotUpdateTotal() ) );
    connect( model, SIGNAL( rowsInserted( const QModelIndex&, int, int ) ),
             SLOT( slotConfigureUi() ) );
    connect( model, SIGNAL( rowsRemoved( const QModelIndex&, int, int ) ),
             SLOT( slotConfigureUi() ) );
    connect( model, SIGNAL( layoutChanged() ),
             SLOT( slotUpdateCurrent() ) );
    connect( model, SIGNAL( modelReset() ),
             SLOT( slotUpdateTotal() ) );

    m_model = model;
    // normally, the model is set only once, so this should be no problem:
    EventEditorDelegate* delegate =
        new EventEditorDelegate( model, m_ui->listView );
    m_ui->listView->setItemDelegate( delegate );


    m_dirty = false;
}

void EventView::slotEventDoubleClicked( const QModelIndex& index )
{
	Q_ASSERT( m_model ); // otherwise, how can we get a doubleclick on an item?
	const Event& event = m_model->eventForIndex( index );
	slotEditEvent( event );
}

void EventView::slotEditEvent( const Event& event )
{
	EventEditor editor( event, this );
	if( editor.exec() ) {
		Event newEvent = editor.event();
        CommandModifyEvent* command =
            new CommandModifyEvent( newEvent, this );
        emitCommand( command );
        m_eventDisplay->setEvent( newEvent );
	}
}


#include "EventView.moc"
