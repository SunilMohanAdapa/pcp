/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007-2008, Aconex.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include "main.h"
#include <QtCore/QTimer>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>
#include <QtGui/QPalette>
#include <qwt_plot.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include "openviewdialog.h"
#include "recorddialog.h"

#define DESPERATE 0

Tab::Tab(): QWidget(NULL)
{
    my.count = 0;
    my.splitter = NULL;
    my.current = -1;
    my.charts = NULL;
    my.group = NULL;
    my.samples = globalSettings.sampleHistory;
    my.visible = globalSettings.visibleHistory;
    my.realDelta = 0;
    my.realPosition = 0;
    my.timeData = NULL;
    my.recording = false;
    my.timeState = Tab::StartState;
    my.buttonState = TimeButton::Timeless;
    my.kmtimeState = KmTime::StoppedState;
    memset(&my.delta, 0, sizeof(struct timeval));
    memset(&my.position, 0, sizeof(struct timeval));
}

void Tab::init(QTabWidget *tab, int samples, int visible,
		QmcGroup *group, KmTime::Source source, QString label,
		struct timeval *interval, struct timeval *position)
{
    my.splitter = new QSplitter(tab);
    my.splitter->setOrientation(Qt::Vertical);
    my.splitter->setSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::MinimumExpanding);
    tab->addTab(my.splitter, label);
    my.group = group;

    my.kmtimeSource = source;
    if (my.kmtimeSource == KmTime::HostSource) {
	my.kmtimeState = KmTime::ForwardState;
	my.buttonState = TimeButton::ForwardLive;
    }
    else {
	my.kmtimeState = KmTime::StoppedState;
	my.buttonState = TimeButton::StoppedArchive;
    }

    my.samples = samples;
    my.visible = visible;
    my.delta = *interval;
    my.position = *position;
    my.realDelta = tosec(*interval);
    my.realPosition = tosec(*position);

    my.timeData = (double *)malloc(samples * sizeof(double));
    for (int i = 0; i < samples; i++)
	my.timeData[i] = my.realPosition - (i * my.realDelta);
}

Chart *Tab::chart(int i)
{
    if (i >= 0 && i < my.count)
	return my.charts[i];
    return NULL;
}

Chart *Tab::addChart(void)
{
    if (my.count)
	kmchart->updateHeight(KmChart::minimumChartHeight());
    Chart *cp = new Chart(this, my.splitter);
    if (!cp)
	nomem();
    my.count++;
    my.charts = (Chart **)realloc(my.charts, my.count * sizeof(Chart *));
    my.charts[my.count-1] = cp;
    setCurrent(cp);
    my.current = my.count - 1;
    console->post("Tab::addChart: [%d]->Chart %p", my.current, cp);
    return cp;
}

int Tab::deleteCurrent(void)
{
    return deleteChart(my.current);
}

int Tab::deleteChart(Chart *cp)
{
    for (int i = 0; i < my.count - 1; i++)
	if (my.charts[i] == cp)
	    return deleteChart(i);
    return 0;
}

int Tab::deleteChart(int idx)
{
    Chart *cp = my.charts[idx];
    int newCurrent = my.current;

    if (my.count > 1)
	kmchart->updateHeight(-(cp->height()));
    delete cp;

    // shuffle left, don't bother with the realloc()
    for (int i = idx; i < my.count - 1; i++)
	my.charts[i] = my.charts[i+1];
    if (idx < newCurrent || idx == my.count - 1) {
	// old current slot no longer available, choose previous slot
	newCurrent--;
    }
    my.count--;
    my.current = -1;		// force re-assignment and re-highlighting
    setCurrent(my.charts[newCurrent]);
    return my.current;
}

int Tab::numChart(void)
{
    return my.count;
}

bool Tab::isArchiveSource(void)
{
    return my.kmtimeSource == KmTime::ArchiveSource;
}

int Tab::currentChartIndex(void)
{
    return my.current;
}

Chart *Tab::currentChart(void)
{
    return my.charts[my.current];
}

void Tab::setCurrent(Chart *cp)
{
    QwtScaleWidget *sp;
    QwtText t;
    QPalette palette;
    int i;

    for (i = 0; i < my.count; i++)
	if (my.charts[i] == cp)
	    break;
    if (i == my.count || i == my.current)
	return;

    console->post("Tab::setCurrentChart(%p) -> %d", cp, i);
    if (my.current != -1) {
	// reset highlight for old current
	t = my.charts[my.current]->titleLabel()->text();
	t.setColor("black");
	my.charts[my.current]->setTitle(t);
	palette = my.charts[my.current]->titleLabel()->palette();
	palette.setColor(QPalette::Active, QPalette::Text, QColor("black"));
        my.charts[my.current]->titleLabel()->setPalette(palette);
	sp = my.charts[my.current]->axisWidget(QwtPlot::yLeft);
	t = sp->title();
	t.setColor("black");
	sp->setTitle(t);
    }
    my.current = i;
    // set title and y-axis highlight for new current
    // for title, have to set both QwtText and QwtTextLabel because of
    // the way attributes are cached and restored when printing charts
    //
    t = cp->titleLabel()->text();
    t.setColor(globalSettings.chartHighlight);
    cp->setTitle(t);
    my.charts[my.current]->setTitle(t);
    palette = my.charts[my.current]->titleLabel()->palette();
    palette.setColor(QPalette::Active, QPalette::Text, globalSettings.chartHighlight);
    my.charts[my.current]->titleLabel()->setPalette(palette);
    sp = cp->axisWidget(QwtPlot::yLeft);
    t = sp->title();
    t.setColor(globalSettings.chartHighlight);
    sp->setTitle(t);
}

void Tab::updateBackground()
{
    for (int i = 0; i < my.count; i++) {
	my.charts[i]->setCanvasBackground(globalSettings.chartBackground);
	my.charts[i]->replot();
    }
}

QmcGroup *Tab::group()
{
    return my.group;
}

void Tab::updateTimeAxis(void)
{
    QString tz, otz, unused;

    if (my.group->numContexts() > 0 || isArchiveSource() == false) {
	if (my.group->numContexts() > 0)
	    my.group->defaultTZ(unused, otz);
	else
	    otz = QmcSource::localHost;
	tz = otz;
	kmchart->timeAxis()->setAxisScale(QwtPlot::xBottom,
		my.timeData[my.visible - 1], my.timeData[0],
		kmchart->timeAxis()->scaleValue(my.realDelta, my.visible));
	kmchart->setDateLabel(my.position.tv_sec, tz);
	kmchart->timeAxis()->replot();
    } else {
	kmchart->timeAxis()->noArchiveSources();
	kmchart->setDateLabel(tr("[No open archives]"));
    }

    if (console->logLevel(KmChart::DebugProtocol)) {
	int i = my.visible - 1;
	console->post(KmChart::DebugProtocol,
			"Tab::updateTimeAxis: tz=%s; visible points=%d",
			(const char *)tz.toAscii(), i);
	console->post(KmChart::DebugProtocol,
			"Tab::updateTimeAxis: first time is %.3f (%s)",
			my.timeData[i], timeString(my.timeData[i]));
	console->post(KmChart::DebugProtocol,
			"Tab::updateTimeAxis: final time is %.3f (%s)",
			my.timeData[0], timeString(my.timeData[0]));
    }
}

void Tab::updateTimeButton(void)
{
    kmchart->setButtonState(my.buttonState);
}

KmTime::State Tab::kmtimeState(void)
{
    return my.kmtimeState;
}

char *Tab::timeState()
{
    static char buf[16];

    switch (my.timeState) {
    case StartState:	strcpy(buf, "Start"); break;
    case ForwardState:	strcpy(buf, "Forward"); break;
    case BackwardState:	strcpy(buf, "Backward"); break;
    case EndLogState:	strcpy(buf, "EndLog"); break;
    case StandbyState:	strcpy(buf, "Standby"); break;
    default:		strcpy(buf, "Dodgey"); break;
    }
    return buf;
}

//
// Drive all updates into each chart (refresh the display)
//
void Tab::refreshCharts(void)
{
#if DESPERATE
    for (int s = 0; s < me.samples; s++)
	console->post(KmChart::DebugProtocol,
			"Tab::refreshCharts: timeData[%2d] is %.2f (%s)",
			s, my.timeData[s], timeString(my.timeData[s]));
    console->post(KmChart::DebugProtocol,
			"Tab::refreshCharts: state=%s", timeState());
#endif

    for (int i = 0; i < my.count; i++) {
	my.charts[i]->setAxisScale(QwtPlot::xBottom, 
		my.timeData[my.visible - 1], my.timeData[0],
		kmchart->timeAxis()->scaleValue(my.realDelta, my.visible));
	my.charts[i]->update(my.timeState != Tab::BackwardState, true);
    }

    if (this == kmchart->activeTab()) {
	updateTimeButton();
	updateTimeAxis();
    }
}

//
// Create the initial scene on opening a view, and show it.
// Most of the work is in archive mode, in live mode we've
// got no historical data that we can display yet.
//
void Tab::setupWorldView(void)
{
    if (isArchiveSource()) {
	KmTime::Packet packet;
	packet.source = KmTime::ArchiveSource;
	packet.state = KmTime::ForwardState;
	packet.mode = KmTime::NormalMode;
	memcpy(&packet.delta, kmtime->archiveInterval(), sizeof(packet.delta));
	memcpy(&packet.position, kmtime->archivePosition(),
						sizeof(packet.position));
	memcpy(&packet.start, kmtime->archiveStart(), sizeof(packet.start));
	memcpy(&packet.end, kmtime->archiveEnd(), sizeof(packet.end));
	adjustWorldView(&packet, true);
    }
    for (int m = 0; m < my.count; m++)
	my.charts[m]->show();
}

//
// Received a Set or a VCRMode requiring us to adjust our state
// and possibly rethink everything.  This can result from a time
// control position change, delta change, direction change, etc.
//
void Tab::adjustWorldView(KmTime::Packet *packet, bool vcrMode)
{
    my.delta = packet->delta;
    my.position = packet->position;
    my.realDelta = tosec(packet->delta);
    my.realPosition = tosec(packet->position);

    console->post("Tab::adjustWorldView: "
		  "sh=%d vh=%d delta=%.2f position=%.2f (%s) state=%s",
		my.samples, my.visible, my.realDelta, my.realPosition,
		timeString(my.realPosition), timeState());

    KmTime::State state = packet->state;
    if (isArchiveSource())
	adjustArchiveWorldView(packet, vcrMode);
    else if (state != KmTime::StoppedState)
	adjustLiveWorldView(packet);
    else {
	newButtonState(state, packet->mode, my.group->mode(), my.recording);
	updateTimeButton();
    }
}

void Tab::adjustArchiveWorldView(KmTime::Packet *packet, bool needFetch)
{
    if (packet->state == KmTime::ForwardState)
	adjustArchiveWorldViewForward(packet, needFetch);
    else if (packet->state == KmTime::BackwardState)
	adjustArchiveWorldViewBackward(packet, needFetch);
    else
	adjustArchiveWorldViewStop(packet, needFetch);
}

static bool fuzzyTimeMatch(double a, double b, double tolerance)
{
    // a matches b if the difference is within 1% of the delta (tolerance)
    return (a == b ||
	    (b > a && a + tolerance > b) ||
	    (b < a && a - tolerance < b));
}

void Tab::adjustLiveWorldView(KmTime::Packet *packet)
{
    console->post("Tab::adjustLiveWorldView");

    //
    // X-Axis _max_ becomes packet->position.
    // Rest of (preceeding) time window filled in using packet->delta.
    // In live mode, we can only fetch current data.  However, we make
    // an effort to keep old data that happens to align with the delta
    // time points (or near enough) that we are now interested in.  So,
    // "oi" is the old index, whereas "i" is the new timeData[] index.
    // First we try to find a fuzzy match on current old index, if it
    // doesn't exit, we continue moving "oi" until it points at a time
    // larger than the one we're after, and then see how that fares on
    // the next iteration.
    //
    int i, oi, last = my.samples - 1;
    bool preserve = false;
    double tolerance = my.realDelta;
    double position = my.realPosition - (my.realDelta * my.samples);

    for (i = oi = last; i >= 0; i--, position += my.realDelta) {
	while (i > 0 && my.timeData[oi] < position + my.realDelta && oi > 0) {
	    if (fuzzyTimeMatch(my.timeData[oi], position, tolerance) == false) {
#if DESPERATE
		console->post("NO fuzzyTimeMatch %.3f to %.3f (%s)",
			my.timeData[oi], position, timeString(position));
#endif
		if (my.timeData[oi] > position)
		    break;
		oi--;
		continue;
	    }
	    console->post("Saved live data (oi=%d/i=%d) for %s", oi, i,
						timeString(position));
	    for (int j = 0; j < my.count; j++)
		my.charts[j]->preserveLiveData(i, oi);
	    my.timeData[i] = my.timeData[oi];
	    preserve = true;
	    oi--;
	    break;
	}

	if (i == 0) {	// refreshCharts() finishes up last one
	    console->post("Fetching data[%d] at %s", i, timeString(position));
	    my.timeData[i] = position;
	    my.group->fetch();
	}
	else if (preserve == false) {
#if DESPERATE
	    console->post("No live data for %s", timeString(position));
#endif
	    my.timeData[i] = position;
	    for (int j = 0; j < my.count; j++)
		my.charts[j]->punchoutLiveData(i);
	}
	else
	    preserve = false;
    }
    // One (per-chart) recalculation & refresh at the end, after all data moved
    for (int j = 0; j < my.count; j++)
	my.charts[j]->adjustedLiveData();
    my.timeState = (packet->state == KmTime::StoppedState) ?
			Tab::StandbyState : Tab::ForwardState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::adjustArchiveWorldViewForward(KmTime::Packet *packet, bool setup)
{
    console->post("Tab::adjustArchiveWorldViewForward");
    my.timeState = Tab::ForwardState;

    int setmode = PM_MODE_INTERP;
    int delta = packet->delta.tv_sec;
    if (packet->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + packet->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    //
    // X-Axis _max_ becomes packet->position.
    // Rest of (preceeding) time window filled in using packet->delta.
    //
    int last = my.samples - 1;
    double tolerance = my.realDelta / 20.0;	// 5% of the sample interval
    double position = my.realPosition - (my.realDelta * last);

    for (int i = last; i >= 0; i--, position += my.realDelta) {
	if (setup == false &&
	    fuzzyTimeMatch(my.timeData[i], position, tolerance) == true) {
	    continue;
	}

	my.timeData[i] = position;

	struct timeval timeval;
	fromsec(position, &timeval);
	my.group->setArchiveMode(setmode, &timeval, delta);
	console->post("Fetching data[%d] at %s", i, timeString(position));
	my.group->fetch();
	if (i == 0)		// refreshCharts() finishes up last one
	    break;
	console->post("Tab::adjustArchiveWorldViewForward: "
		      "setting time position[%d]=%.2f[%s] state=%s count=%d",
			i, position, timeString(position),
			timeState(), my.count);
	for (int j = 0; j < my.count; j++)
	    my.charts[j]->update(true, false);
    }

    if (setup)
	packet->state = KmTime::StoppedState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::adjustArchiveWorldViewBackward(KmTime::Packet *packet, bool setup)
{
    console->post("Tab::adjustArchiveWorldViewBackward");
    my.timeState = Tab::BackwardState;

    int setmode = PM_MODE_INTERP;
    int delta = packet->delta.tv_sec;
    if (packet->delta.tv_usec == 0) {
	setmode |= PM_XTB_SET(PM_TIME_SEC);
    } else {
	delta = delta * 1000 + packet->delta.tv_usec / 1000;
	setmode |= PM_XTB_SET(PM_TIME_MSEC);
    }

    //
    // X-Axis _min_ becomes packet->position.
    // Rest of (following) time window filled in using packet->delta.
    //
    int last = my.samples - 1;
    double tolerance = my.realDelta / 20.0;	// 5% of the sample interval
    double position = my.realPosition;

    for (int i = 0; i <= last; i++, position -= my.realDelta) {
	if (setup == false &&
	    fuzzyTimeMatch(my.timeData[i], position, tolerance) == true) {
	    continue;
	}

	my.timeData[i] = position;

	struct timeval timeval;
	fromsec(position, &timeval);
	my.group->setArchiveMode(setmode, &timeval, -delta);
	console->post("Fetching data[%d] at %s", i, timeString(position));
	my.group->fetch();
	if (i == last)		// refreshCharts() finishes up last one
	    break;
	console->post("Tab::adjustArchiveWorldViewBackward: "
		      "setting time position[%d]=%.2f[%s] state=%s count=%d",
			i, position, timeString(position),
			timeState(), my.count);
	for (int j = 0; j < my.count; j++)
	    my.charts[j]->update(false, false);
    }

    if (setup)
	packet->state = KmTime::StoppedState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::adjustArchiveWorldViewStop(KmTime::Packet *packet, bool needFetch)
{
    if (needFetch) {	// stopped, but VCR reposition event occurred
	adjustArchiveWorldViewForward(packet, needFetch);
	return;
    }
    my.timeState = Tab::StandbyState;
    packet->state = KmTime::StoppedState;
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    updateTimeButton();
}

//
// Catch the situation where we get a larger than expected increase
// in position.  This happens when we restart after a stop in live
// mode (both with and without a change in the delta).
//
static bool sideStep(double n, double o, double interval)
{
    // tolerance set to 5% of the sample interval:
    return fuzzyTimeMatch(o + interval, n, interval/20.0) == false;
}

//
// Fetch all metric values across all plots and all charts,
// and also update the single time scale across the bottom.
//
void Tab::step(KmTime::Packet *packet)
{
    double stepPosition = tosec(packet->position);

    console->post(KmChart::DebugProtocol,
		  "Tab::step: stepping to time %.2f, delta=%.2f, state=%s",
		  stepPosition, my.realDelta, timeState());

    if ((packet->source == KmTime::ArchiveSource &&
	((packet->state == KmTime::ForwardState &&
		my.timeState != Tab::ForwardState) ||
	 (packet->state == KmTime::BackwardState &&
		my.timeState != Tab::BackwardState))) ||
	 sideStep(stepPosition, my.realPosition, my.realDelta))
	return adjustWorldView(packet, false);

    int last = my.samples - 1;
    my.kmtimeState = packet->state;
    my.position = packet->position;
    my.realPosition = stepPosition;

    if (packet->state == KmTime::ForwardState) { // left-to-right (all but 1st)
	if (my.samples > 1)
	    memmove(&my.timeData[1], &my.timeData[0], sizeof(double) * last);
	my.timeData[0] = my.realPosition;
    }
    else if (packet->state == KmTime::BackwardState) { // right-to-left
	if (my.samples > 1)
	    memmove(&my.timeData[0], &my.timeData[1], sizeof(double) * last);
	my.timeData[last] = my.realPosition - torange(my.delta, last);
    }

    my.group->fetch();
    newButtonState(packet->state, packet->mode, my.group->mode(), my.recording);
    refreshCharts();
}

void Tab::VCRMode(KmTime::Packet *packet, bool dragMode)
{
    if (!dragMode)
	adjustWorldView(packet, true);
}

void Tab::setTimezone(char *tz)
{
    console->post(KmChart::DebugProtocol, "Tab::setTimezone - %s", tz);
    my.group->useTZ(QString(tz));
    if (this == kmchart->activeTab())
	updateTimeAxis();
}

void Tab::setSampleHistory(int v)
{
    console->post("Tab::setSampleHistory (%d -> %d)", my.samples, v);
    if (my.samples != v) {
	my.samples = v;
	for (int i = 0; i < my.count; i++)
	    for (int m = 0; m < my.charts[i]->numPlot(); m++)
		my.charts[i]->resetDataArrays(m, my.samples);
	my.timeData = (double *)malloc(my.samples * sizeof(my.timeData[0]));
	if (my.timeData == NULL)
	    nomem();
	for (int i = 0; i < my.samples; i++)
	    my.timeData[i] = my.realPosition - (i * my.realDelta);
    }
}

int Tab::sampleHistory(void)
{
    return my.samples;
}

void Tab::setVisibleHistory(int v)
{
    console->post("Tab::setVisibleHistory (%d -> %d)", my.visible, v);
    if (my.visible != v) {
	my.visible = v;
    }
}

int Tab::visibleHistory(void)
{
    return my.visible;
}

double *Tab::timeAxisData(void)
{
    return my.timeData;
}

bool Tab::isRecording(void)
{
    return my.recording;
}

bool Tab::startRecording(void)
{
    RecordDialog record(this);

    console->post("Tab::startRecording");
    record.init(this);
    if (record.exec() != QDialog::Accepted)
	my.recording = false;
    else {	// write pmlogger/kmchart/pmafm configs and start up loggers.
	console->post("Tab::startRecording starting loggers");
	record.startLoggers();
	my.recording = true;
    }
    return my.recording;
}

void Tab::stopRecording(void)
{
    QString msg = "Q\n", errmsg;
    int count = my.loggerList.size();
    int i, sts, error = 0;

    console->post(KmChart::DebugForce, "Tab::stopRecording stopping %d logger(s)", count);
    for (int i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(tr("Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error++;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	    my.loggerList.at(i)->terminate();
	}
    }

    for (i = 0; i < my.archiveList.size(); i++) {
	QString archive = my.archiveList.at(i);

	console->post(KmChart::DebugForce, "Tab::stopRecording opening archive %s",
			(const char *)archive.toAscii());
	if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	    errmsg.append(tr("Cannot open PCP archive: "));
	    errmsg.append(archive);
	    errmsg.append("\n");
	    errmsg.append(tr(pmErrStr(sts)));
	    errmsg.append("\n");
	    error++;
	}
	else {
	    archiveGroup->updateBounds();
	    QmcSource source = archiveGroup->context()->source();
	    kmtime->addArchive(source.start(), source.end(),
				source.timezone(), source.host(), true);
	}
    }

    // If all is well, we can now create the new "Record" Tab.
    // Order of cleanup and changing Record mode state is different
    // in the error case to non-error case, this is important for
    // getting the window state correct (i.e. kmchart->enableUi()).

    if (error) {
	cleanupRecording();
	kmchart->setRecordState(this, false);
	QMessageBox::warning(this, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	// Make the current Tab stop recording before changing Tabs
	kmchart->setRecordState(this, false);

	Tab *tab = new Tab;
	console->post("Tab::stopRecording creating tab: delta=%.2f pos=%.2f",
			tosec(*kmtime->archiveInterval()),
			tosec(*kmtime->archivePosition()));
	tab->init(kmchart->tabWidget(), my.samples, my.visible,
		  archiveGroup, KmTime::ArchiveSource, tr("Record"),
		  kmtime->archiveInterval(), kmtime->archivePosition());
	kmchart->addActiveTab(tab);
	OpenViewDialog::openView((const char *)my.view.toAscii());
	cleanupRecording();
    }
}

void Tab::cleanupRecording(void)
{
    my.recording = false;
    my.loggerList.clear();
    my.archiveList.clear();
    my.view = QString::null;
    my.folio = QString::null;
}

void Tab::queryRecording(void)
{
    QString msg = "?\n", errmsg;
    int i, error = 0, count = my.loggerList.size();

    console->post("Tab::stopRecording querying %d logger(s)", count);
    for (i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(tr("Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error++;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	}
    }

    if (error) {
	msg = "Q\n";	// if one fails, we shut down all loggers
	for (i = 0; i < count; i++)
	    my.loggerList.at(i)->write(msg.toAscii());
	cleanupRecording();
	kmchart->setRecordState(this, false);
	QMessageBox::warning(this, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
}

void Tab::detachLoggers(void)
{
    QString msg = "D\n", errmsg;
    int error = 0, count = my.loggerList.size();

    console->post("Tab::detachLoggers detaching %d logger(s)", count);
    for (int i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(tr("Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error++;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	}
    }

    if (error) {
	cleanupRecording();
	kmchart->setRecordState(this, false);
	QMessageBox::warning(this, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	kmchart->setRecordState(this, false);
	cleanupRecording();
    }
}

void Tab::addFolio(QString folio, QString view)
{
    my.view = view;
    my.folio = folio;
}

void Tab::addLogger(PmLogger *pmlogger, QString archive)
{
    my.loggerList.append(pmlogger);
    my.archiveList.append(archive);
}

TimeButton::State Tab::buttonState(void)
{
    return my.buttonState;
}

void Tab::newButtonState(KmTime::State s, KmTime::Mode m, int src, bool record)
{
    if (src != PM_CONTEXT_ARCHIVE) {
	if (s == KmTime::StoppedState)
	    my.buttonState = record ?
			TimeButton::StoppedRecord : TimeButton::StoppedLive;
	else
	    my.buttonState = record ?
			TimeButton::ForwardRecord : TimeButton::ForwardLive;
    }
    else if (m == KmTime::StepMode) {
	if (s == KmTime::ForwardState)
	    my.buttonState = TimeButton::StepForwardArchive;
	else if (s == KmTime::BackwardState)
	    my.buttonState = TimeButton::StepBackwardArchive;
	else
	    my.buttonState = TimeButton::StoppedArchive;
    }
    else if (m == KmTime::FastMode) {
	if (s == KmTime::ForwardState)
	    my.buttonState = TimeButton::FastForwardArchive;
	else if (s == KmTime::BackwardState)
	    my.buttonState = TimeButton::FastBackwardArchive;
	else
	    my.buttonState = TimeButton::StoppedArchive;
    }
    else if (s == KmTime::ForwardState)
	my.buttonState = TimeButton::ForwardArchive;
    else if (s == KmTime::BackwardState)
	my.buttonState = TimeButton::BackwardArchive;
    else
	my.buttonState = TimeButton::StoppedArchive;
}
