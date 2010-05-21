/*
 *  Copyright (C) 2010  Alexandre Courbot
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

#include "gui/EntriesViewHelper.h"
#include "gui/EntryMenu.h"
#include "gui/EditEntryNotesDialog.h"
#include "gui/TagsDialogs.h"
#include "gui/EntriesPrinter.h"

#include <QAction>
#include <QProgressDialog>
#include <QFile>
#include <QPrintDialog>
#include <QFileDialog>
#include <QMessageBox>

EntriesViewHelper::EntriesViewHelper(QAbstractItemView* client, EntryDelegateLayout* delegateLayout, bool workOnSelection, bool viewOnly) : EntryMenu(client), _client(client), _entriesMenu(), _workOnSelection(workOnSelection), _actionPrint(QIcon(":/images/icons/print.png"), tr("&Print..."), 0), _actionPrintPreview(QIcon(":/images/icons/print.png"), tr("Print p&review..."), 0), _actionPrintBooklet(QIcon(":/images/icons/print.png"), tr("Print &booklet..."), 0), _actionPrintBookletPreview(QIcon(":/images/icons/print.png"), tr("Booklet p&review..."), 0), _actionExportTab(QIcon(":/images/icons/document-export.png"), tr("&Export..."), 0), _actionExportJs(tr("Export as &HTML..."), 0), prefRefs(MAX_PREF), _contextMenu()
{
	// If no delegate layout has been specified, let's use our private one...
	if (!delegateLayout) delegateLayout = new EntryDelegateLayout(this);
	connect(delegateLayout, SIGNAL(layoutHasChanged()), this, SLOT(updateLayout()));
	_delegateLayout = delegateLayout;

	connect(&addToStudyAction, SIGNAL(triggered()), this, SLOT(studySelected()));
	connect(&removeFromStudyAction, SIGNAL(triggered()), this, SLOT(unstudySelected()));
	connect(&alreadyKnownAction, SIGNAL(triggered()), this, SLOT(markAsKnown()));
	connect(&resetTrainingAction, SIGNAL(triggered()), this, SLOT(resetTraining()));
	connect(&setTagsAction, SIGNAL(triggered()), this, SLOT(setTags()));
	connect(&addTagsAction, SIGNAL(triggered()), this, SLOT(addTags()));
	connect(&setNotesAction, SIGNAL(triggered()), this, SLOT(addNote()));
	connect(this, SIGNAL(tagsHistorySelected(const QStringList &)), this, SLOT(addTags(const QStringList &)));

	connect(&_actionPrint, SIGNAL(triggered()), this, SLOT(print()));
	connect(&_actionPrintBooklet, SIGNAL(triggered()), this, SLOT(printBooklet()));
	connect(&_actionPrintPreview, SIGNAL(triggered()), this, SLOT(printPreview()));
	connect(&_actionPrintBookletPreview, SIGNAL(triggered()), this, SLOT(printBookletPreview()));
	connect(&_actionExportTab, SIGNAL(triggered()), this, SLOT(tabExport()));
	connect(&_actionExportJs, SIGNAL(triggered()), this, SLOT(jsExport()));
		
	_entriesMenu.addAction(&_actionPrint);
	_entriesMenu.addAction(&_actionPrintPreview);
	_entriesMenu.addAction(&_actionPrintBooklet);
	_entriesMenu.addAction(&_actionPrintBookletPreview);
	_entriesMenu.addSeparator();
	_entriesMenu.addAction(&_actionExportTab);
	_entriesMenu.addAction(&_actionExportJs);
	
	// If the view is editable, the helper menu shall be enabled
	if (!viewOnly) {
		_contextMenu.addSeparator();
		populateMenu(&_contextMenu);
		_contextMenu.addSeparator();
	}
}

void EntriesViewHelper::setPreferenceHandler(Preference pref, PreferenceRoot *ref)
{
	if (prefRefs[pref]) disconnect(prefRefs[pref], SIGNAL(valueChanged(QVariant)), _delegateLayout, SLOT(updateConfig(QVariant)));
	prefRefs[pref] = ref;
	if (ref) {
		_delegateLayout->setProperty(ref->name().toLatin1().constData(), ref->variantValue());
		connect(prefRefs[pref], SIGNAL(valueChanged(QVariant)), _delegateLayout, SLOT(updateConfig(QVariant)));
	}
}

QList<EntryPointer> EntriesViewHelper::selectedEntries() const
{
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	QList<EntryPointer> selectedEntries;
	foreach(const QModelIndex &index, selection) {
		EntryPointer entry(qVariantValue<EntryPointer>(index.data(Entry::EntryRole)));
		if (entry) selectedEntries << entry;
	}
	return selectedEntries;
}

void EntriesViewHelper::studySelected()
{
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	// Progress bar
	QProgressDialog progressDialog(tr("Marking entries..."), tr("Abort"), 0, selection.size(), client());
	progressDialog.setMinimumDuration(1000);
	progressDialog.setWindowTitle(tr("Operation in progress..."));
	progressDialog.setWindowModality(Qt::WindowModal);

	int i = 0;
	foreach (const QModelIndex &index, selection) {
		if (progressDialog.wasCanceled()) break;
		progressDialog.setValue(i++);
		EntryPointer entry = qVariantValue<EntryPointer>(index.data(Entry::EntryRole));
		if (!entry) continue;
		entry->addToTraining();
		client()->update(index);
	}
}

void EntriesViewHelper::unstudySelected()
{
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	// Progress bar
	QProgressDialog progressDialog(tr("Marking entries..."), tr("Abort"), 0, selection.size(), client());
	progressDialog.setMinimumDuration(1000);
	progressDialog.setWindowTitle(tr("Operation in progress..."));
	progressDialog.setWindowModality(Qt::WindowModal);

	int i = 0;
	foreach (const QModelIndex &index, selection) {
		if (progressDialog.wasCanceled()) break;
		progressDialog.setValue(i++);
		EntryPointer entry = qVariantValue<EntryPointer>(index.data(Entry::EntryRole));
		if (!entry) continue;
		entry->removeFromTraining();
		client()->update(index);
	}
}

void EntriesViewHelper::markAsKnown()
{
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	// Progress bar
	QProgressDialog progressDialog(tr("Marking entries..."), tr("Abort"), 0, selection.size(), client());
	progressDialog.setMinimumDuration(1000);
	progressDialog.setWindowTitle(tr("Operation in progress..."));
	progressDialog.setWindowModality(Qt::WindowModal);

	int i = 0;
	foreach (const QModelIndex &index, selection) {
		if (progressDialog.wasCanceled()) break;
		progressDialog.setValue(i++);
		EntryPointer entry = qVariantValue<EntryPointer>(index.data(Entry::EntryRole));
		if (!entry) continue;
		if (entry->alreadyKnown()) continue;
		entry->setAlreadyKnown();
		client()->update(index);
	}
}

void EntriesViewHelper::resetTraining()
{
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	// Progress bar
	QProgressDialog progressDialog(tr("Resetting entries..."), tr("Abort"), 0, selection.size(), client());
	progressDialog.setMinimumDuration(1000);
	progressDialog.setWindowTitle(tr("Operation in progress..."));
	progressDialog.setWindowModality(Qt::WindowModal);

	int i = 0;
	foreach (const QModelIndex &index, selection) {
		if (progressDialog.wasCanceled()) break;
		progressDialog.setValue(i++);
		EntryPointer entry = qVariantValue<EntryPointer>(index.data(Entry::EntryRole));
		if (!entry) continue;
		entry->resetScore();
		client()->update(index);
	}
}

void EntriesViewHelper::setTags()
{
	TagsDialogs::setTagsDialog(selectedEntries(), client());
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	foreach (const QModelIndex &index, selection)
		client()->update(index);
}

void EntriesViewHelper::addTags()
{
	TagsDialogs::addTagsDialog(selectedEntries(), client());
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	foreach (const QModelIndex &index, selection)
		client()->update(index);
}

void EntriesViewHelper::addTags(const QStringList &tags)
{
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	// Progress bar
	QProgressDialog progressDialog(tr("Adding tags..."), tr("Abort"), 0, selection.size(), client());
	progressDialog.setMinimumDuration(1000);
	progressDialog.setWindowTitle(tr("Operation in progress..."));
	progressDialog.setWindowModality(Qt::WindowModal);
	
	int i = 0;
	foreach (const QModelIndex &index, selection) {
		if (progressDialog.wasCanceled()) break;
		progressDialog.setValue(i++);
		EntryPointer entry = qVariantValue<EntryPointer>(index.data(Entry::EntryRole));
		if (!entry) continue;
		entry->addTags(tags);
		client()->update(index);
	}
}

void EntriesViewHelper::addNote()
{
	QModelIndexList selection = client()->selectionModel()->selectedIndexes();
	if (selection.size() != 1) return;
	EntryPointer entry = qVariantValue<EntryPointer >(selection[0].data(Entry::EntryRole));
	if (!entry) return;
	EditEntryNotesDialog dialog(*entry, client());
	if (dialog.exec() != QDialog::Accepted) return;
	foreach (const QModelIndex &index, selection)
		client()->update(index);
}

bool EntriesViewHelper::askForPrintOptions(QPrinter &printer, const QString &title)
{
	QPrintDialog printDialog(&printer, client());
	printDialog.setWindowTitle(title);
	QAbstractPrintDialog::PrintDialogOptions options = QAbstractPrintDialog::PrintToFile | QAbstractPrintDialog::PrintPageRange;
	if (!_workOnSelection) options |= QAbstractPrintDialog::PrintSelection;
	printDialog.setOptions(options);
	if (printDialog.exec() != QDialog::Accepted) return false;
	return true;
}

QModelIndexList EntriesViewHelper::getAllIndexes(const QModelIndexList& indexes)
{
	QSet<QModelIndex> alreadyIn;
	return getAllIndexes(indexes, alreadyIn);
}

static bool modelIndexLessThan(const QModelIndex &i1, const QModelIndex &i2)
{
	QModelIndex p1(i1.parent());
	QModelIndex p2(i2.parent());
	if (i1 == p2) return true;
	else if (p1 == p2) {
		return (i1.row() < i2.row());
	}
	else if (p1.isValid() && modelIndexLessThan(p1, i2)) return true;
	return false;
}

QModelIndexList EntriesViewHelper::getAllIndexes(const QModelIndexList& indexes, QSet<QModelIndex>& alreadyIn)
{
	QModelIndexList ret;
	
	foreach (const QModelIndex &idx, indexes) {
		// Should never happen
		if (!idx.isValid()) continue;
		if (alreadyIn.contains(idx)) continue;
		ret << idx;
		alreadyIn << idx;
		// Entries can be put directly into the return list
		ConstEntryPointer entry(qVariantValue<EntryPointer>(idx.data(Entry::EntryRole)));
		if (!entry) {
			// Non entries indexes must be list - see if they have children
			QModelIndexList childs;
			int childsCount = idx.model()->rowCount(idx);
			for (int i = 0; i < childsCount; i++) childs << idx.child(i, 0);
			ret += getAllIndexes(childs, alreadyIn);
		}
	}
	qStableSort(ret.begin(), ret.end(), modelIndexLessThan);
	return ret;
}

QModelIndexList EntriesViewHelper::getEntriesToProcess(bool limitToSelection)
{
	QModelIndexList selIndexes;
	if (_workOnSelection || limitToSelection) selIndexes = client()->selectionModel()->selectedIndexes();
	else for (int i = 0; i < client()->model()->rowCount(QModelIndex()); i++) {
		selIndexes << client()->model()->index(i, 0, QModelIndex());
	}
	QModelIndexList entries(getAllIndexes(selIndexes));
	return entries;
}
	

void EntriesViewHelper::print()
{
	QPrinter printer;
	if (!askForPrintOptions(printer)) return;
	EntriesPrinter(client()).print(getEntriesToProcess(printer.printRange() & QPrinter::Selection), &printer);
}

void EntriesViewHelper::printPreview()
{
	QPrinter printer;
	EntriesPrinter(client()).printPreview(getEntriesToProcess(), &printer);
}

void EntriesViewHelper::printBooklet()
{
	QPrinter printer;
	if (!askForPrintOptions(printer, tr("Booklet print"))) return;
	EntriesPrinter(client()).printBooklet(getEntriesToProcess(printer.printRange() & QPrinter::Selection), &printer);
}

void EntriesViewHelper::printBookletPreview()
{
	QPrinter printer;
	EntriesPrinter(client()).printBookletPreview(getEntriesToProcess(), &printer);
}

void EntriesViewHelper::tabExport()
{
	QString exportFile = QFileDialog::getSaveFileName(0, tr("Export to tab-separated file..."));
	if (exportFile.isEmpty()) return;
	QFile outFile(exportFile);
	if (!outFile.open(QIODevice::WriteOnly)) {
		QMessageBox::warning(0, tr("Cannot write file"), QString(tr("Unable to write file %1.")).arg(exportFile));
		return;
	}

	QModelIndexList entries(getEntriesToProcess());

	// Dummy entry to notify Anki that tab is our delimiter
	//outFile.write("\t\t\n");
	foreach (const QModelIndex &idx, entries) {
		ConstEntryPointer entry = qVariantValue<EntryPointer>(idx.data(Entry::EntryRole));
		// We cannot "export" lists due to the file purpose
		if (!entry) continue;
		QStringList writings = entry->writings();
		QStringList readings = entry->readings();
		QStringList meanings = entry->meanings();
		QString writing;
		QString reading;
		QString meaning;
		if (writings.size() > 0) writing = writings[0];
		if (readings.size() > 0) reading = readings[0];
		if (meanings.size() == 1) meaning += " " + meanings[0];
		else {
			int cpt = 1;
			foreach (const QString &str, meanings)
				meaning += QString(" (%1) %2").arg(cpt++).arg(str);
		}
		if (outFile.write(QString("%1\t%2\t%3\n").arg(writing).arg(readings.join(", ")).arg(meanings.join(", ")).toUtf8()) == -1) {
			QMessageBox::warning(0, tr("Error writing file"), QString(tr("Error while writing file %1.")).arg(exportFile));
			return;
		}
	}

	outFile.close();
}

void EntriesViewHelper::jsExport()
{
	QString exportFile = QFileDialog::getSaveFileName(0, tr("Export to HTML flashcard file..."), QString(), tr("HTML files (*.html)"));
	if (exportFile.isEmpty()) return;
	QFile outFile(exportFile);
	if (!outFile.open(QIODevice::WriteOnly)) {
		QMessageBox::warning(0, tr("Cannot write file"), QString(tr("Unable to write file %1!")).arg(exportFile));
		return;
	}

	QModelIndexList entries(getEntriesToProcess());
	QList<ConstEntryPointer> realEntries;

	foreach (const QModelIndex &idx, entries) {
		ConstEntryPointer entry = qVariantValue<EntryPointer>(idx.data(Entry::EntryRole));
		// We cannot "export" lists due to the file purpose
		if (!entry) continue;
		realEntries << entry;
	}
	
	QFile tmplFile(":/images/export_template.html");
	if (!tmplFile.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(0, tr("Cannot open template file"), QString(tr("Unable to open template file!")).arg(exportFile));
		return;
	}
	
	QString tmpl(QString::fromUtf8(tmplFile.readAll()));
	
	int idx = 0;
	QString data(QString("var entries = Array();\nfor (var i = 0; i < %1; i++) entries[i] = Array();\n").arg(realEntries.size()).toUtf8());
	foreach (const ConstEntryPointer &entry, realEntries) {
		QStringList writings = entry->writings();
		QStringList readings = entry->readings();
		QStringList meanings = entry->meanings();
		QString writing;
		QString reading;
		QString meaning;
		if (writings.size() > 0) writing = writings[0];
		if (readings.size() > 0) reading = readings[0];
		if (meanings.size() == 1) meaning += " " + meanings[0];
		else {
			int cpt = 1;
			foreach (const QString &str, meanings)
				meaning += QString(" (%1) %2").arg(cpt++).arg(str);
		}
		data += QString("entries[%1][0]=\"%2\";\nentries[%1][1]=\"%3\";\nentries[%1][2]=\"%4\";\n").arg(idx++).arg(writings.join(", ")).arg(readings.join(", ")).arg(meanings.join(", "));
	}
	
	tmpl.replace("__DATA__", data);
	outFile.write(tmpl.toUtf8());

	outFile.close();
}

void EntriesViewHelper::updateLayout()
{
	// This is needed to force a redraw - but we loose the selection.
	QAbstractItemModel *m = client()->model();
	client()->setModel(0);
	client()->setModel(m);
}

void EntriesViewHelper::updateConfig(const QVariant &value)
{
	PreferenceRoot *from = qobject_cast<PreferenceRoot *>(sender());
	if (!from) return;
	client()->setProperty(from->name().toLatin1().constData(), value);
}
