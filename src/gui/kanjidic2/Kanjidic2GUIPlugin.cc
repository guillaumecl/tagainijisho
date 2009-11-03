/*
 *  Copyright (C) 2009  Alexandre Courbot
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "core/TextTools.h"
#include "gui/BetterSpinBox.h"
#include "gui/KanjiValidator.h"
#include "gui/TrainSettings.h"
#include "gui/kanjidic2/Kanjidic2EntryFormatter.h"
#include "gui/kanjidic2/KanjiPopup.h"
#include "gui/MainWindow.h"
#include "gui/SearchWidget.h"
#include "gui/kanjidic2/Kanjidic2Preferences.h"
#include "gui/kanjidic2/Kanjidic2GUIPlugin.h"
// TODO BAD - dependency against JMdict!
#include "gui/jmdict/JMdictGUIPlugin.h"

#include <QVBoxLayout>
#include <QFrame>
#include <QMessageBox>
#include <QApplication>
#include <QDesktopWidget>
#include <QInputDialog>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QToolTip>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>

Kanjidic2GUIPlugin::Kanjidic2GUIPlugin() : Plugin("kanjidic2GUI"), _formatter(0), _flashKL(0), _flashKS(0), _flashML(0), _flashMS(0), _readingPractice(0), _linkHandler(0), _wordsLinkHandler(0), _componentsLinkHandler(0), _extender(0), _trainer(0), _readingTrainer(0)
{
}

Kanjidic2GUIPlugin::~Kanjidic2GUIPlugin()
{
}

bool Kanjidic2GUIPlugin::onRegister()
{
	// Check if the kanjidic plugin is loaded
	if (!Plugin::pluginExists("kanjidic2")) return false;
	// Register the formatter
	_formatter = new Kanjidic2EntryFormatter();
	if (!EntryFormatter::registerFormatter(KANJIDIC2ENTRY_GLOBALID, _formatter)) { delete _formatter; return false; }
	// Register the link handlers
	_linkHandler = new KanjiLinkHandler();
	if (!DetailedViewLinkManager::registerHandler(_linkHandler)) return false;
	_wordsLinkHandler = new KanjiAllWordsHandler();
	if (!DetailedViewLinkManager::registerHandler(_wordsLinkHandler)) return false;
	_componentsLinkHandler = new KanjiAllComponentsOfHandler();
	if (!DetailedViewLinkManager::registerHandler(_componentsLinkHandler)) return false;

	// Add the main window entries
	MainWindow *mainWindow(MainWindow::instance());
	QMenu *menu = mainWindow->trainMenu();
	if (!menu->isEmpty()) menu->addSeparator();
	QMenu *menu2 = menu->addMenu(tr("Kanji flashcards"));
	_flashKL = menu2->addAction(tr("From &kanji, whole study list"));
	_flashKS = menu2->addAction(tr("From &kanji, current set"));
	connect(_flashKL, SIGNAL(triggered()), this, SLOT(trainingKanjiList()));
	connect(_flashKS, SIGNAL(triggered()), this, SLOT(trainingKanjiSet()));
	menu2->addSeparator();
	_flashML = menu2->addAction(tr("From &meaning, whole study list"));
	_flashMS = menu2->addAction(tr("From &meaning, current set"));
	connect(_flashML, SIGNAL(triggered()), this, SLOT(trainingMeaningList()));
	connect(_flashMS, SIGNAL(triggered()), this, SLOT(trainingMeaningSet()));
	menu->addSeparator();
	_readingPractice = menu->addAction(tr("&Reading practice, whole study list"));
	connect(_readingPractice, SIGNAL(triggered()), this, SLOT(readingPractice()));

	// Register the searchbar extender
	_extender = new Kanjidic2OptionsWidget(0);
	mainWindow->searchWidget()->searchBar()->registerExtender(_extender);

	// Register the detailed view event filter
	DetailedView::registerEventFilter(this);

	// Register the preferences panel
	PreferencesWindow::addPanel(&Kanjidic2Preferences::staticMetaObject);

	return true;
}

bool Kanjidic2GUIPlugin::onUnregister()
{
	// Remove the preferences panel
	PreferencesWindow::removePanel(&Kanjidic2Preferences::staticMetaObject);

	// Remove the detailed view event filter
	DetailedView::removeEventFilter(this);

	MainWindow *mainWindow = MainWindow::instance();
	// Remove the search extender
	mainWindow->searchWidget()->searchBar()->removeExtender(_extender);
	delete _extender; _extender = 0;
	// Remove the main window entries
	delete _flashKL; _flashKL = 0;
	delete _flashKS; _flashKS = 0;
	delete _flashML; _flashML = 0;
	delete _flashMS; _flashMS = 0;
	delete _readingPractice; _readingPractice = 0;
	// Remove the link handlers
	DetailedViewLinkManager::removeHandler(_linkHandler);
	delete _linkHandler; _linkHandler = 0;
	DetailedViewLinkManager::removeHandler(_wordsLinkHandler);
	delete _wordsLinkHandler; _wordsLinkHandler = 0;
	DetailedViewLinkManager::removeHandler(_componentsLinkHandler);
	delete _componentsLinkHandler; _componentsLinkHandler = 0;
	// Remove the formatter
	EntryFormatter::removeFormatter(KANJIDIC2ENTRY_GLOBALID);
	delete _formatter; _formatter = 0;
	return true;
}

QString Kanjidic2GUIPlugin::pluginInfo() const
{
	return QString::fromUtf8("<p>The <a href=\"http://www.csse.monash.edu.au/~jwb/kanjidic.html\">Kanjidic</a> is distributed under the <a href=\"http://creativecommons.org/licenses/by-sa/3.0/\">Creative Common Attribution Share Alike Licence, version 3.0</a>.</p><p>Kanji stroke animations are from the <a href=\"http://kanjivg.tagaini.net/\">KanjiVG</a> project and are distributed under the <a href=\"http://creativecommons.org/licenses/by-sa/3.0/\">Creative Commons Attribution-Share Alike 3.0 licence</a>.</p><p>JLPT levels are taken from the <a href=\"http://www.jlptstudy.com/\">JLPT Study Page</a> and lists provided by <a href=\"http://www.thbz.org/kanjimots/jlpt.php3\">Thierry Bézecourt</a> and <a href=\"http://jetsdencredujapon.blogspot.com\">Alain Côté</a>, with kind permission.");
}

// TODO duplicate code from JMdictGUIPlugin
void Kanjidic2GUIPlugin::training(YesNoTrainer::TrainingMode mode, const QString &queryString)
{
	bool restart = false;
	// Trainer is automatically set to 0 by the destroyed() slot
	if (_trainer && (_trainer->trainingMode() != mode || _trainer->query() != queryString)) delete _trainer;
	if (!_trainer) {
		restart = true;
		_trainer = new JMdictYesNoTrainer(MainWindow::instance());
		_trainer->setAttribute(Qt::WA_DeleteOnClose);
		_trainer->setWindowFlags(Qt::Window);
		connect(_trainer, SIGNAL(destroyed()), this, SLOT(trainerDeleted()));
		_trainer->setTrainingMode(mode);
		_trainer->setQuery(queryString);
	}

	_trainer->show();
	_trainer->activateWindow();
	_trainer->raise();

	if (restart) _trainer->train();
}

void Kanjidic2GUIPlugin::trainingKanjiList()
{
	training(YesNoTrainer::Japanese, TrainSettings::buildQueryString(KANJIDIC2ENTRY_GLOBALID));
}

void Kanjidic2GUIPlugin::trainingKanjiSet()
{
	QueryBuilder qBuilder(MainWindow::instance()->searchWidget()->queryBuilder());
	const QueryBuilder::Statement *stat(qBuilder.getStatementForEntryType(KANJIDIC2ENTRY_GLOBALID));
	if (!stat) {
		QMessageBox::information(MainWindow::instance(), tr("Nothing to train"), tr("There are no kanji entries in this set to train on."));
		return;
	}

	QString queryString(stat->buildSqlStatement());
	queryString += " " + TrainSettings::buildOrderString("3");
	training(YesNoTrainer::Japanese, queryString);
}

void Kanjidic2GUIPlugin::trainingMeaningList()
{
	training(YesNoTrainer::Translation, TrainSettings::buildQueryString(KANJIDIC2ENTRY_GLOBALID));}

void Kanjidic2GUIPlugin::trainingMeaningSet()
{
	QueryBuilder qBuilder(MainWindow::instance()->searchWidget()->queryBuilder());
	const QueryBuilder::Statement *stat(qBuilder.getStatementForEntryType(KANJIDIC2ENTRY_GLOBALID));
	if (!stat) {
		QMessageBox::information(MainWindow::instance(), tr("Nothing to train"), tr("There are no kanji entries in this set to train on."));
		return;
	}

	QString queryString(stat->buildSqlStatement());
	queryString += " " + TrainSettings::buildOrderString("3");
	training(YesNoTrainer::Translation, queryString);
}

void Kanjidic2GUIPlugin::trainerDeleted()
{
	_trainer = 0;
}

void Kanjidic2GUIPlugin::readingPractice()
{
	bool restart = false;
	if (!_readingTrainer) {
		restart = true;
		_readingTrainer = new ReadingTrainer(MainWindow::instance());
		_readingTrainer->setAttribute(Qt::WA_DeleteOnClose);
		_readingTrainer->setWindowFlags(Qt::Window);
		connect(_readingTrainer, SIGNAL(destroyed()), this, SLOT(readingTrainerDeleted()));
		_readingTrainer->newSession();
	}

	_readingTrainer->show();
	_readingTrainer->activateWindow();
	_readingTrainer->raise();

	if (restart) _readingTrainer->train();
}

void Kanjidic2GUIPlugin::readingTrainerDeleted()
{
	_readingTrainer = 0;
}

bool Kanjidic2GUIPlugin::eventFilter(QObject *obj, QEvent *_event)
{
	DetailedView *view(qobject_cast<DetailedView *>(obj->parent()));
	if (!view) return false;
	int pos = view->document()->documentLayout()->hitTest(view->viewport()->mapFromGlobal(QCursor::pos() + QPoint(0, view->verticalScrollBar()->value())), Qt::ExactHit);
	switch(_event->type()) {
 case QEvent::MouseMove:
		{
			// TODO this property should be moved out of view
			if (view->kanjisClickable()) {
				if (pos != -1) {
					QTextCursor cursor(view->document());
					cursor.setPosition(pos);
					cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
					if (!cursor.charFormat().isAnchor()) {
						QChar t(cursor.selectedText()[0]);
						QString c(t);
						if (t.isHighSurrogate()) {
							cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
							c += cursor.selectedText()[1];
						}
						if (TextTools::isKanjiChar(c)) {
							EntryPointer<Entry> entry(EntriesCache::get(KANJIDIC2ENTRY_GLOBALID, TextTools::singleCharToUnicode(c)));
							// Only show the tooltip if the entry exists in the database!
							if (entry.data()) {
								view->viewport()->setCursor(QCursor(Qt::PointingHandCursor));
								const EntryFormatter *formatter(EntryFormatter::getFormatter(entry.data()));
								formatter->showToolTip(entry.data(), QCursor::pos());
								return true;
							} else return false;
						}
					} else return false;
				}
				view->viewport()->unsetCursor();
				if (QToolTip::isVisible()) {
					QToolTip::hideText();
				}
				return false;
			}
			break;
		}
 case QEvent::MouseButtonPress:
		{
			QMouseEvent *event(static_cast<QMouseEvent *>(_event));
			if (!view->kanjisClickable()) return false;
			if (event->button() != Qt::LeftButton) return false;

			if (pos != -1) {
				QTextCursor cursor(view->document());
				cursor.setPosition(pos);
				cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
				if (cursor.charFormat().isAnchor()) return false;
				QChar t(cursor.selectedText()[0]);
				QString c(t);
				if (t.isHighSurrogate()) {
					cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
					c += cursor.selectedText()[1];
				}
				if (TextTools::isKanjiChar(c)) {
					view->fakeClick(QUrl(QString("drawkanji:?kanji=%1").arg(c)));
					return true;
				}
			}
			break;
		}
 case QEvent::ContextMenu:
		{
			QContextMenuEvent *event(static_cast<QContextMenuEvent *>(_event));
			QMenu *menu = 0;
			SingleEntryView tview;

			// Are we on an entry link or a kanji character?
			if (pos != -1) {
				QTextCursor cursor(view->document());
				cursor.setPosition(pos);
				cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
				if (cursor.charFormat().isAnchor()) {
					QUrl url(cursor.charFormat().anchorHref());
					if (url.scheme() == "entry") {
						EntryPointer<Entry> entry(EntriesCache::get(url.queryItemValue("type").toInt(), url.queryItemValue("id").toInt()));
						if (entry.data()) tview.setEntry(entry.data());
					}
				}
				else {
					QChar c(cursor.selectedText()[0]);
					if (TextTools::isKanjiChar(c)) {
						EntryPointer<Entry> entry(EntriesCache::get(KANJIDIC2ENTRY_GLOBALID, c.unicode()));
						if (entry.data()) tview.setEntry(entry.data());
					}
				}
			}

			if (tview.entry()) {
				menu = new QMenu();
				tview.populateMenu(menu);
			}
			else return false;

			menu->exec(event->globalPos());
			delete menu;
			return true;
		}
 default:
		break;
	}
	return false;
}

KanjiLinkHandler::KanjiLinkHandler() : DetailedViewLinkHandler("drawkanji")
{
}

void KanjiLinkHandler::handleUrl(const QUrl &url, DetailedView *view)
{
	QString kanji = url.queryItemValue("kanji");
	if (kanji.isEmpty()) return;

	// Do it here so that the popup appears at the position we clicked, even if loading
	// takes time and we moved the mouse.
	QRect windowRect = QApplication::activeWindow()->frameGeometry();

	EntryPointer<Entry> entryPtr = EntriesCache::get(KANJIDIC2ENTRY_GLOBALID, TextTools::singleCharToUnicode(kanji));
	Kanjidic2Entry *entry = static_cast<Kanjidic2Entry *>(entryPtr.data());
	if (!entry) return;

	KanjiPopup *popup = new KanjiPopup();
	QObject::connect(popup, SIGNAL(requestDisplay(Entry*)), MainWindow::instance()->searchWidget()->detailedView(), SLOT(display(Entry*)));
	popup->display(entry);

	popup->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	popup->setWindowModality(Qt::ApplicationModal);
	popup->setWindowTitle(tr("Tracing for %1").arg(kanji));
	popup->setAttribute(Qt::WA_DeleteOnClose);
	popup->setWindowFlags(Qt::Popup);
	// Must be called now so we know the actual geometry of the window
//	popup->adjustSize();

	QPoint pos = QCursor::pos();
	QPoint lowerRight = pos + QPoint(popup->size().width(), popup->size().height());
	if (!windowRect.contains(QPoint(lowerRight.x(), pos.y()))) pos.setX(windowRect.right() - popup->width());
	if (!windowRect.contains(QPoint(pos.x(), lowerRight.y()))) pos.setY(windowRect.bottom() - popup->height());

	popup->move(pos);
	popup->show();

	pos = QCursor::pos();
	lowerRight = pos + QPoint(popup->size().width(), popup->size().height());
	if (!windowRect.contains(QPoint(lowerRight.x(), pos.y()))) pos.setX(windowRect.right() - popup->width());
	if (!windowRect.contains(QPoint(pos.x(), lowerRight.y()))) pos.setY(windowRect.bottom() - popup->height());

	// Do it again to ensure the geometry is correctly calculated after show()
	popup->move(pos);
}

KanjiAllWordsHandler::KanjiAllWordsHandler() : DetailedViewLinkHandler("allwords")
{
}

void KanjiAllWordsHandler::handleUrl(const QUrl &url, DetailedView *view)
{
	SearchBar *searchBar = MainWindow::instance()->searchWidget()->searchBar();
	EntryTypeSelectionWidget *entryType = qobject_cast<EntryTypeSelectionWidget *>(searchBar->getExtender("entrytypeselector"));
	JMdictOptionsWidget *extender = qobject_cast<JMdictOptionsWidget *>(searchBar->getExtender("jmdictoptions"));
	if (!entryType || !extender) return;

//	searchBar->reset();
	entryType->setAutoUpdateQuery(false);
	entryType->setType(EntryTypeSelectionWidget::Vocabulary);
	entryType->setAutoUpdateQuery(true);
	extender->setAutoUpdateQuery(false);
	extender->setContainedKanjis(url.queryItemValue("kanji"));
	extender->setAutoUpdateQuery(true);
	searchBar->search();
}

KanjiAllComponentsOfHandler::KanjiAllComponentsOfHandler() : DetailedViewLinkHandler("component")
{
}

void KanjiAllComponentsOfHandler::handleUrl(const QUrl &url, DetailedView *view)
{
	SearchBar *searchBar = MainWindow::instance()->searchWidget()->searchBar();
	EntryTypeSelectionWidget *entryType = qobject_cast<EntryTypeSelectionWidget *>(searchBar->getExtender("entrytypeselector"));
	Kanjidic2OptionsWidget *extender = qobject_cast<Kanjidic2OptionsWidget *>(searchBar->getExtender("kanjidicoptions"));
	if (!entryType || !extender) return;

//	searchBar->reset();
	entryType->setAutoUpdateQuery(false);
	entryType->setType(EntryTypeSelectionWidget::Kanjis);
	entryType->setAutoUpdateQuery(true);
	extender->setAutoUpdateQuery(false);
	extender->setComponents(url.queryItemValue("kanji"));
	extender->setAutoUpdateQuery(true);
	searchBar->search();
}

Kanjidic2OptionsWidget::Kanjidic2OptionsWidget(QWidget *parent) : SearchBarExtender(parent, "kanjidic")
{
	_propsToSave << "strokeCount" << "components" << "unicode" << "grades";

	QGroupBox *_strokeCountGroupBox = new QGroupBox(tr("Stroke count"), this);
	connect(_strokeCountGroupBox, SIGNAL(toggled(bool)), this, SLOT(commandUpdate()));
	{
		QHBoxLayout *hLayout = new QHBoxLayout(_strokeCountGroupBox);
		_strokeCountSpinBox = new BetterSpinBox(this);
		_strokeCountSpinBox->setSpecialValueText("");
		_strokeCountSpinBox->setRange(0, 34);
		hLayout->addWidget(_strokeCountSpinBox);
		connect(_strokeCountSpinBox, SIGNAL(valueChanged(QString)), this, SLOT(delayedCommandUpdate()));
	}
	QGroupBox *componentsGroupBox = new QGroupBox(tr("Components"), this);
	{
		QHBoxLayout *hLayout = new QHBoxLayout(componentsGroupBox);
		_components = new QLineEdit(componentsGroupBox);
		KanjiValidator *kanjiValidator = new KanjiValidator(_components);
		_components->setValidator(kanjiValidator);
		connect(_components, SIGNAL(textChanged(const QString &)), this, SLOT(commandUpdate()));
		hLayout->addWidget(_components);
	}
	QGroupBox *codeGroupBox = new QGroupBox(tr("Unicode"), this);
	{
		QHBoxLayout *hLayout = new QHBoxLayout(codeGroupBox);
		_code = new HexSpinBox(codeGroupBox);
		_code->setRange(0, 0x2A6DF);
		_code->setPrefix("0x");
		connect(_code, SIGNAL(valueChanged(int)), this, SLOT(delayedCommandUpdate()));
		hLayout->addWidget(_code);
	}
	QGroupBox *gradeGroupBox = new QGroupBox(tr("School grade"), this);
	{
		QHBoxLayout *hLayout = new QHBoxLayout(gradeGroupBox);
		_gradeButton = new QPushButton(tr("Grade"), gradeGroupBox);
		QMenu *menu = new QMenu(_gradeButton);
		QAction *action;
		actionGroup = new QActionGroup(menu);
		actionGroup->setExclusive(false);
		action = actionGroup->addAction(tr("Elementary &1st grade"));
		action->setProperty("Agrade", 1);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("Elementary &2nd grade"));
		action->setProperty("Agrade", 2);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("Elementary &3rd grade"));
		action->setProperty("Agrade", 3);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("Elementary &4th grade"));
		action->setProperty("Agrade", 4);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("Elementary &5th grade"));
		action->setProperty("Agrade", 5);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("Elementary &6th grade"));
		action->setProperty("Agrade", 6);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("&Secondary school"));
		action->setProperty("Agrade", 8);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("Used for names (1)"));
		action->setProperty("Agrade", 9);
		action->setCheckable(true);
		action = actionGroup->addAction(tr("Used for names (2)"));
		action->setProperty("Agrade", 10);
		action->setCheckable(true);
		connect(actionGroup, SIGNAL(triggered(QAction *)), this, SLOT(onGradeTriggered(QAction *)));
		menu->addActions(actionGroup->actions());
		menu->addSeparator();
		allKyouku = menu->addAction(tr("All &Kyouiku kanjis"));
		allKyouku->setCheckable(true);
		connect(allKyouku, SIGNAL(toggled(bool)), this, SLOT(allKyoukuKanjis(bool)));
		allJouyou = menu->addAction(tr("All &Jouyou kanjis"));
		allJouyou->setCheckable(true);
		connect(allJouyou, SIGNAL(toggled(bool)), this, SLOT(allJouyouKanjis(bool)));

		_gradeButton->setMenu(menu);
		hLayout->addWidget(_gradeButton);
	}
	QHBoxLayout *mainLayout = new QHBoxLayout(this);
	mainLayout->addWidget(_strokeCountGroupBox);
	mainLayout->addWidget(componentsGroupBox);
	mainLayout->addWidget(codeGroupBox);
	mainLayout->addWidget(gradeGroupBox);
	mainLayout->setContentsMargins(0, 0, 0, 0);
}

void Kanjidic2OptionsWidget::allKyoukuKanjis(bool checked)
{
	bool prevStatus = allJouyou->blockSignals(true);
	allJouyou->setChecked(false);
	allJouyou->blockSignals(prevStatus);
	_gradesList.clear();
	foreach(QAction *action, actionGroup->actions()) {
		int grade = action->property("Agrade").toInt();
		if (grade <= 6) {
			action->setChecked(checked);
			if (checked) _gradesList << QString::number(grade);
		}
		else action->setChecked(false);
	}
	actionGroup->setEnabled(!checked);
	commandUpdate();
}

void Kanjidic2OptionsWidget::allJouyouKanjis(bool checked)
{
	bool prevStatus = allKyouku->blockSignals(true);
	allKyouku->setChecked(false);
	allKyouku->blockSignals(prevStatus);
	_gradesList.clear();
	foreach(QAction *action, actionGroup->actions()) {
		int grade = action->property("Agrade").toInt();
		if (grade <= 8) {
			action->setChecked(checked);
			if (checked) _gradesList << QString::number(grade);
		}
		else action->setChecked(false);
	}
	actionGroup->setEnabled(!checked);
	commandUpdate();
}

QString Kanjidic2OptionsWidget::currentCommand() const
{
	QString ret;

	if (_strokeCountSpinBox->value() > 0) {
		ret += QString(" :stroke=%1").arg(_strokeCountSpinBox->value());
	}
	QString kanjis = _components->text();
	if (!kanjis.isEmpty()) {
		bool first = true;
		ret += " :component=";
		foreach(QChar c, kanjis) {
			if (!first) ret +=",";
			else first = false;
			ret += QString("\"%1\"").arg(c);
		}
	}
	if (_code->value()) ret += QString(" :unicode=%1").arg(_code->text());
	if (!_gradesList.isEmpty()) ret += " :grade=" + _gradesList.join(",");
	return ret;
}

QString Kanjidic2OptionsWidget::currentTitle() const
{
	QString ret;

	QString kanjis = _components->text();
	if (!kanjis.isEmpty()) {
		bool first = true;
		ret += "[";
		foreach(QChar c, kanjis) {
			if (!first) ret +=",";
			else first = false;
			ret += c;
		}
		ret += "]";
	}
	if (_strokeCountSpinBox->value() > 0) {
		if (_strokeCountSpinBox->value() == 1) ret += tr(", 1 stroke");
		else ret += tr(", %1 strokes").arg(_strokeCountSpinBox->value());
	}
	if (_code->value()) ret += tr(", unicode: ") + _code->text();
	if (!_gradesList.isEmpty()) ret += tr(", grade:") + _gradesList.join(",");
	if (!ret.isEmpty()) ret = tr("Kanji") + ret;
	else ret = tr("Kanjis");
	return ret;
}

void Kanjidic2OptionsWidget::onGradeTriggered(QAction *action)
{
	if (action->isChecked()) {
		int grade = action->property("Agrade").toInt();
		_gradesList << QString::number(grade);
	} else {
		int grade = action->property("Agrade").toInt();
		_gradesList.removeOne(QString::number(grade));
	}
	if (!_gradesList.isEmpty()) _gradeButton->setText(tr("Grade:") + _gradesList.join(","));
	else _gradeButton->setText(tr("Grade"));
	commandUpdate();
}

void Kanjidic2OptionsWidget::updateFeatures()
{
	if (_strokeCountSpinBox->value() || !_components->text().isEmpty() || !_gradesList.isEmpty() || _code->value()) emit disableFeature("wordsdic");
	else emit enableFeature("wordsdic");
}

void Kanjidic2OptionsWidget::_reset()
{
	_strokeCountSpinBox->setValue(0);
	_code->setValue(0);
	_gradesList.clear();
	foreach (QAction *action, _gradeButton->menu()->actions()) if (action->isChecked()) action->trigger();
	_components->clear();
}

void Kanjidic2OptionsWidget::setGrades(const QStringList &list)
{
	_gradesList.clear();
	foreach(QAction *action, _gradeButton->menu()->actions()) {
		if (action->isChecked()) action->trigger();
		if (list.contains(action->property("Agrade").toString())) action->trigger();
	}
}