/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: UIFilePathSelector class declaration.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIFilePathSelector_h___
#define ___UIFilePathSelector_h___

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Qt includes: */
#include <QComboBox>

/* Forward declarations: */
class QILabel;
class QILineEdit;
class QHBoxLayout;
class QAction;
class QIToolButton;


/** QComboBox extension providing GUI with
  * possibility to choose/reflect file/folder path. */
class UIFilePathSelector: public QIWithRetranslateUI<QComboBox>
{
    Q_OBJECT;

public:

    /** Modes file-path selector operates in. */
    enum Mode
    {
        Mode_Folder = 0,
        Mode_File_Open,
        Mode_File_Save
    };

    /** Constructs file-path selector passing @a pParent to QComboBox base-class. */
    UIFilePathSelector(QWidget *pParent = 0);
    /** Destructs file-path selector. */
    ~UIFilePathSelector();

    /** Defines the @a enmMode to operate in. */
    void setMode(Mode enmMode);
    /** Returns the mode to operate in. */
    Mode mode() const;

    /** Defines whether the path is @a fEditable. */
    void setEditable(bool fEditable);
    /** Returns whether the path is editable. */
    bool isEditable() const;

    /** Defines whether the reseting to defauilt path is @a fEnabled. */
    void setResetEnabled(bool fEnabled);
    /** Returns whether the reseting to defauilt path is enabled. */
    bool isResetEnabled() const;

    /** Defines the file-dialog @a strTitle. */
    void setFileDialogTitle(const QString &strTitle);
    /** Returns the file-dialog title. */
    QString fileDialogTitle() const;

    /** Defines the file-dialog @a strFilters. */
    void setFileDialogFilters(const QString &strFilters);
    /** Returns the file-dialog filters. */
    QString fileDialogFilters() const;

    /** Defines the file-dialog @a strDefaultSaveExtension. */
    void setFileDialogDefaultSaveExtension(const QString &strDefaultSaveExtension);
    /** Returns the file-dialog default save extension. */
    QString fileDialogDefaultSaveExtension() const;

    /** Resets path modified state to false. */
    void resetModified();
    /** Returns whether the path is modified. */
    bool isModified() const;
    /** Returns whether the path is selected. */
    bool isPathSelected() const;

    /** Returns the path. */
    QString path() const;

signals:

    /** Notify listeners about @a strPath changed. */
    void pathChanged(const QString &strPath);

public slots:

    /** Defines the @a strPath and @a fRefreshText after that. */
    void setPath(const QString &strPath, bool fRefreshText = true);

    /** Defines the @a strHomeDir. */
    void setHomeDir(const QString &strHomeDir);

protected:

    /** Handles resize @a pEvent. */
    void resizeEvent(QResizeEvent *pEvent);

    /** Handles focus-in @a pEvent. */
    void focusInEvent(QFocusEvent *pEvent);
    /** Handles focus-out @a pEvent. */
    void focusOutEvent(QFocusEvent *pEvent);

    /** Preprocesses every @a pEvent sent to @a pObject. */
    bool eventFilter(QObject *pObject, QEvent *pEvent);

    /** Handles translation event. */
    void retranslateUi();

private slots:

    /** Handles combo-box @a iIndex activation. */
    void onActivated(int iIndex);

    /** Handles combo-box @a strText editing. */
    void onTextEdited(const QString &strText);

    /** Handles combo-box text copying. */
    void copyToClipboard();

    /** Refreshes combo-box text according to chosen path. */
    void refreshText();

private:

    /** Provokes change to @a strPath and @a fRefreshText after that. */
    void changePath(const QString &strPath, bool fRefreshText = true);

    /** Call for file-dialog to choose path. */
    void selectPath();

    /** Returns default icon. */
    QIcon defaultIcon() const;

    /** Returns full path @a fAbsolute if necessary. */
    QString fullPath(bool fAbsolute = true) const;

    /** Shrinks the reflected text to @a iWidth pixels. */
    QString shrinkText(int iWidth) const;

    /** Holds the copy action instance. */
    QAction *m_pCopyAction;

    /** Holds the mode to operate in. */
    Mode     m_enmMode;

    /** Holds the path. */
    QString  m_strPath;
    /** Holds the home dir. */
    QString  m_strHomeDir;

    /** Holds the file-dialog filters. */
    QString  m_strFileDialogFilters;
    /** Holds the file-dialog default save extension. */
    QString  m_strFileDialogDefaultSaveExtension;
    /** Holds the file-dialog title. */
    QString  m_strFileDialogTitle;

    /** Holds the cached text for empty path. */
    QString  m_strNoneText;
    /** Holds the cached tool-tip for empty path. */
    QString  m_strNoneToolTip;

    /** Holds whether the path is editable. */
    bool     m_fEditable;

    /** Holds whether we are in editable mode. */
    bool     m_fEditableMode;
    /** Holds whether we are expecting mouse events. */
    bool     m_fMouseAwaited;

    /** Holds whether the path is modified. */
    bool     m_fModified;
};

#endif /* !___UIFilePathSelector_h___ */

