/*
  CharmReport.h

  This file is part of Charm, a task-based time tracking application.

  Copyright (C) 2014-2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

  Author: Frank Osterfeld <frank.osterfeld@kdab.com>
  Author: Montel Laurent <laurent.montel@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CHARMREPORT_H
#define CHARMREPORT_H

#include <QObject>

#include "Core/Task.h"
#include "Core/Event.h"

class QWidget;

class ReportDialog;
class QTextDocument;
class ParagraphFormatCollection;

class CharmReport : public QObject
{
    Q_OBJECT

public:
    explicit CharmReport( QObject* parent = nullptr );
    virtual ~CharmReport();

    // fetch all necessary data to create the report
    // after calling that, it should not be necessary to query the
    // storage again. create() will make the actual report later.
    virtual bool prepare();

    // create the report.
    // return true if successful, false otherwise
    virtual bool create() = 0;

    // return the report as a QTextDocument
    // this method may change or be removed
    virtual QTextDocument* report() = 0;

    // a literal description of the report
    virtual QString description() = 0;

    // the literal name of the report
    virtual QString name() = 0;

    // the configuration widget for this dialog
    // use the report dialog pointer to connect to it's accept
    // or reject signals
    virtual QWidget* configurationPage( ReportDialog* ) = 0;

signals:
    // emit this to signal that the user has accepted the
    // configuration page:
    void accept();

protected slots:
        /** Create a report window with the content that report() returns,
        and show it. The window will be delete-on-close, so the
        creator does not have tp care for it. It will provide the
        printing functionality to the user. */
    void makeReportPreviewWindow();

protected:
    // access the default paragraph format collection
    ParagraphFormatCollection& paragraphFormats();
};

#endif
