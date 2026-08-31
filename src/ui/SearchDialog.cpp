#include "ui/SearchDialog.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/Theme.h"

namespace devicehub {

SearchDialog::SearchDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Search Messages"));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(ui_theme::kSpacingSm);

    auto* queryRow = new QHBoxLayout;
    queryRow->setSpacing(ui_theme::kSpacingSm);

    queryEdit_ = new QLineEdit(this);
    queryEdit_->setObjectName(QStringLiteral("searchQueryEdit"));
    queryEdit_->setPlaceholderText(tr("Search this channel…"));
    connect(queryEdit_, &QLineEdit::returnPressed, this,
            [this]() { emit searchRequested(queryEdit_->text()); });

    searchButton_ = new QPushButton(tr("Search"), this);
    searchButton_->setObjectName(QStringLiteral("searchDialogSearchButton"));
    searchButton_->setProperty("accent", true);
    connect(searchButton_, &QPushButton::clicked, this, [this]() { emit searchRequested(queryEdit_->text()); });

    queryRow->addWidget(queryEdit_, /*stretch=*/1);
    queryRow->addWidget(searchButton_);

    resultsList_ = new QListWidget(this);
    resultsList_->setObjectName(QStringLiteral("searchResultsList"));
    connect(resultsList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        emit resultActivated(item->data(Qt::UserRole).toLongLong());
    });

    rootLayout->addLayout(queryRow);
    rootLayout->addWidget(resultsList_, /*stretch=*/1);

    resize(420, 360);
}

void SearchDialog::setResults(const QList<ChatMessageInfo>& matches) {
    resultsList_->clear();
    for (const ChatMessageInfo& match : matches) {
        auto* item = new QListWidgetItem(tr("%1: %2 (%3)").arg(match.author, match.body, match.sentAt));
        item->setData(Qt::UserRole, match.id);
        resultsList_->addItem(item);
    }
}

void SearchDialog::clearResults() {
    resultsList_->clear();
    queryEdit_->clear();
}

}  // namespace devicehub
