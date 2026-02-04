#include "NotificationWidget.h"
#include <QPainter>
#include <QApplication>

NotificationWidget::NotificationWidget(QWidget* parent)
	: QWidget(parent)
	, m_parentWidget(parent)
{
	// 设置窗口属性
	this->setAttribute(Qt::WA_ShowWithoutActivating);
	this->setParent(parent);

	// 固定大小
	this->setFixedSize(320, 90);

	// 设置阴影
	auto* shadowEffect = new QGraphicsDropShadowEffect(this);
	shadowEffect->setBlurRadius(20);
	shadowEffect->setColor(QColor(0, 0, 0, 60));
	shadowEffect->setOffset(0, 4);
	this->setGraphicsEffect(shadowEffect);

	this->hide();

	setupUI();
	setupAnimations();
}

void NotificationWidget::setupUI()
{
	// 主布局
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(12, 10, 12, 10);
	mainLayout->setSpacing(6);

	// 标题栏布局
	QHBoxLayout* titleLayout = new QHBoxLayout();
	titleLayout->setContentsMargins(0, 0, 0, 0);
	titleLayout->setSpacing(8);

	// 标题标签
	m_titleLabel = new QLabel(this);
	m_titleLabel->setObjectName("titleLabel");
	m_titleLabel->setStyleSheet(
		"QLabel#titleLabel {"
		"font-weight: bold;"
		"font-size: 13px;"
		"color: #1a1a1a;"
		"padding-left: 4px;"
		"}"
	);

	// 关闭按钮
	m_closeButton = new QPushButton(this);
	m_closeButton->setFixedSize(24, 24);
	m_closeButton->setText("×");
	m_closeButton->setStyleSheet(
		"QPushButton {"
		"border: none;"
		"border-radius: 12px;"
		"font-size: 18px;"
		"font-weight: bold;"
		"background-color: transparent;"
		"color: #999999;"
		"}"
		"QPushButton:hover {"
		"background-color: #f0f0f0;"
		"color: #333333;"
		"}"
		"QPushButton:pressed {"
		"background-color: #e0e0e0;"
		"}"
	);

	connect(m_closeButton, &QPushButton::clicked, this, &NotificationWidget::onCloseClicked);

	titleLayout->addWidget(m_titleLabel);
	titleLayout->addStretch();
	titleLayout->addWidget(m_closeButton);

	// 消息标签
	m_messageLabel = new QLabel(this);
	m_messageLabel->setObjectName("messageLabel");
	m_messageLabel->setStyleSheet(
		"QLabel#messageLabel {"
		"font-size: 12px;"
		"color: #666666;"
		"padding-left: 4px;"
		"}"
	);
	m_messageLabel->setWordWrap(true);
	m_messageLabel->setMaximumHeight(40);

	mainLayout->addLayout(titleLayout);
	mainLayout->addWidget(m_messageLabel);
	mainLayout->addStretch();
}

void NotificationWidget::setupAnimations()
{
	// 显示动画
	m_showAnimation = new QPropertyAnimation(this, "geometry");
	m_showAnimation->setDuration(250);
	m_showAnimation->setEasingCurve(QEasingCurve::OutCubic);

	// 隐藏动画
	m_hideAnimation = new QPropertyAnimation(this, "geometry");
	m_hideAnimation->setDuration(250);
	m_hideAnimation->setEasingCurve(QEasingCurve::InCubic);

	QObject::connect(m_hideAnimation, &QPropertyAnimation::finished, this, &QWidget::hide);

	// 自动关闭定时器
	m_timer = new QTimer(this);
	m_timer->setSingleShot(true);
	QObject::connect(m_timer, &QTimer::timeout, this, &NotificationWidget::hideNotification);
}

void NotificationWidget::calculatePosition()
{
	if (!m_parentWidget) {
		return;
	}

	// 计算相对位置
	int x = m_parentWidget->width() - this->width() - 20;
	int y = 20;

	this->move(x, y);
	this->raise();
}

void NotificationWidget::showNotification(const QString& title, const QString& message,
	NotificationType type, int duration)
{
	// 设置内容
	m_type = type;
	m_titleLabel->setText(title);
	m_messageLabel->setText(message);

	// 计算位置
	calculatePosition();

	this->update();
	// 动画起始和结束位置
	QRect currentRect = this->geometry();
	QRect startRect = currentRect;
	QRect endRect = currentRect;

	// 从右侧滑入
	startRect.moveLeft(m_parentWidget->width());

	m_showAnimation->setStartValue(startRect);
	m_showAnimation->setEndValue(endRect);

	// 显示并开始动画
	this->show();
	this->raise();
	m_showAnimation->start();

	// 设置自动关闭
	if (duration > 0) {
		m_timer->start(duration);
	}
}

void NotificationWidget::hideNotification()
{
	m_timer->stop();

	QRect currentRect = this->geometry();
	QRect endRect = currentRect;
	endRect.moveLeft(currentRect.x() + currentRect.width());

	m_hideAnimation->setStartValue(currentRect);
	m_hideAnimation->setEndValue(endRect);
	m_hideAnimation->start();
}

void NotificationWidget::onCloseClicked()
{
	hideNotification();
}

void NotificationWidget::paintEvent(QPaintEvent* event)
{
	Q_UNUSED(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// 绘制圆角矩形背景
	QRectF rect(0, 0, this->width(), this->height());

	// 背景颜色（鼠标悬停时稍亮）
	QColor backgroundColor = m_isHovered ? QColor(255, 255, 255, 245) : QColor(255, 255, 255, 235);

	painter.setBrush(backgroundColor);
	painter.setPen(QPen(QColor(220, 220, 220, 180), 1));
	painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);

	// 绘制左侧彩色条
	QRectF leftBar(0, 8, 4, this->height() - 16);
	QColor barColor = getColorForType(m_type);

	painter.setBrush(barColor);
	painter.setPen(Qt::NoPen);
	painter.drawRoundedRect(leftBar, 2, 2);
}

void NotificationWidget::enterEvent(QEnterEvent* event)
{
	Q_UNUSED(event);
	m_isHovered = true;
	this->update();  // 触发重绘

	// 鼠标悬停时暂停自动关闭
	if (m_timer && m_timer->isActive()) {
		m_timer->stop();
	}
}

void NotificationWidget::leaveEvent(QEvent* event)
{
	Q_UNUSED(event);
	m_isHovered = false;
	this->update();  // 触发重绘

	// 鼠标离开后重新开始自动关闭（如果有剩余时间）
	if (m_timer) {
		m_timer->start(2000);  // 离开后2秒关闭
	}
}

void NotificationWidget::setOpacity(qreal opacity)
{
	m_opacity = opacity;
	this->setWindowOpacity(opacity);
}

QColor NotificationWidget::getColorForType(NotificationType type) const {
	switch (type) {
	case Info:    return QColor(41, 128, 185);   // 纯蓝色
	case Success: return QColor(39, 174, 96);    // 纯绿色
	case Warning: return QColor(241, 196, 15);   // 纯黄色
	case Error:   return QColor(231, 76, 60);    // 纯红色
	default:      return QColor(41, 128, 185);
	}
}