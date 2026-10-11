"""固定 Loader 事件：等价用例严格逐状态比较，修复用例分别断言旧缺陷与新结果。"""

def event(op, **values):
    return dict(op=op, **values)


def checkpoint(**values):
    return event('settle', expect=values)


def difference(baseline, candidate):
    return event('settle', baseline=baseline, candidate=candidate)


def cases():
    result = []
    def add(name, events, repair=None, asynchronous=False, callbacks=None):
        result.append(dict(name=name, mode='repair' if repair else 'equivalent', repair=repair,
            events=[event('configure', asynchronous=asynchronous, callbacks=callbacks or [])] + events))

    add('sync-initial-properties-reopen-identity', [
        event('open', key='a', properties={'initialInput':'supplied'}),
        checkpoint(opened=1, failures=0, **{'slots.a.page.initial':'supplied', 'slots.a.page.prepared':1, 'slots.a.page.serial':1}),
        event('exit'), event('control'), event('close'), checkpoint(active=False, status=0, keys=['a']),
        event('open', key='a', properties={'initialInput':'ignored'}, keep=False),
        checkpoint(opened=2, created=1, **{'slots.a.retained':True, 'slots.a.page.initial':'supplied'}),
        event('deliver'), checkpoint(opened=2), event('close'), checkpoint(keys=['a'])])
    add('sync-transient-hide-release', [event('open', key='a', keep=False), checkpoint(opened=1),
        event('close'), checkpoint(keys=[], selected=None, item=None), event('open', key='a', keep=False),
        checkpoint(created=2, opened=2), event('close'), checkpoint(keys=[])])
    add('async-warm-open-reopen', [event('warm', key='a'), checkpoint(keys=['a'], opened=0, **{'slots.a.page.prepared':1}),
        event('open', key='a'), checkpoint(opened=1), event('close'), checkpoint(), event('open', key='a'),
        checkpoint(opened=2, created=1)], asynchronous=True)
    add('async-switch-before-ready', [event('open', key='a'), event('select', key='b'),
        checkpoint(opened=1, selected='b', item='b', **{'slots.a.page.prepared':1, 'slots.b.page.prepared':1})], asynchronous=True)
    add('repeat-show-and-manual-delivery', [event('open', key='a'), checkpoint(opened=1),
        event('select', key='a', url='Missing.qml'), event('set', property='active', value=True),
        event('deliver'), checkpoint(opened=1, created=1), event('close'), event('open', key='a'), checkpoint(opened=2)])
    add('async-transient-hide-before-ready', [event('open', key='a', keep=False), event('save'), event('close'),
        event('loaded', saved=True), checkpoint(keys=[], opened=0, selected=None)], asynchronous=True)
    add('source-failure-unlocks-next-page', [event('open', key='missing', url='Missing.qml'),
        checkpoint(opening=False, active=False, opened=0), event('open', key='a'), checkpoint(opened=1, opening=False)])
    add('activation-failure-unlocks-next-page', [event('open', key='bad', properties={'failActivate':True}),
        checkpoint(activationFailures=1, opening=False, active=False), event('open', key='a'), checkpoint(opened=2, active=True)])
    add('loaded-callback-hides', [event('open', key='a'), checkpoint(opened=1, active=False, delivered=False)],
        callbacks=[dict(phase='loaded', token='a', events=[event('close')])])
    add('loaded-callback-reopens', [event('open', key='a'), checkpoint(opened=2, active=True)],
        callbacks=[dict(phase='loaded', token='a', events=[event('set', property='active', value=False),event('set', property='active', value=True)])])
    add('destroy-pool-pending', [event('open', key='a'),event('destroy'),checkpoint(live=False, opened=0)], asynchronous=True)
    add('destroy-pool-in-loaded-callback', [event('open', key='a'),checkpoint(live=False,opened=1)],
        callbacks=[dict(phase='loaded',token='a',events=[event('destroy')])])
    add('warm-keeps-original-key-contract', [event('warm',key='a'),checkpoint(created=1),
        event('open',key='a',keep=False,url='Missing.qml'),checkpoint(opened=1,**{'slots.a.retained':True}),
        event('close'),checkpoint(keys=['a'])])

    add('repair-preparation-failure-reopen', [event('open',key='bad',properties={'failPrepare':True}),
        checkpoint(failures=1,opened=0,active=False,opening=False),event('open',key='bad'),
        difference({'opened':1,'failures':1,'active':True}, {'opened':0,'failures':2,'active':False,'opening':False}),
        event('open',key='good'),difference({'opened':1},{'opened':1,'active':True,'opening':False})],
        repair='Failed preparation remains failed on reopen; no success delivery.',asynchronous=True)
    add('repair-synchronous-preparation-failure', [event('open',key='bad',properties={'failPrepare':True}),
        difference({'opened':1,'failures':0},{'opened':0,'failures':1,'active':False,'opening':False})],
        repair='Synchronous failure before selection is retained and unlocks presentation.')
    add('repair-deactivation-failure-reopen', [event('open',key='bad',properties={'failDeactivate':True}),
        checkpoint(failures=1,opened=0,active=False),event('open',key='bad'),
        difference({'opened':1,'failures':1},{'opened':0,'failures':2,'active':False})],
        repair='residentDeactivate failure also prevents delivery on reopen.',asynchronous=True)
    add('repair-switch-already-ready', [event('warm',key='a'),event('warm',key='b'),checkpoint(opened=0),
        event('open',key='a'),checkpoint(opened=1),event('select',key='b'),
        difference({'opened':1,'selected':'b'},{'opened':2,'selected':'b','item':'b'})],
        repair='Active selection changes start an independent one-shot presentation.')
    add('repair-switch-transient-release-late-loaded', [event('open',key='a',keep=False),checkpoint(opened=1),event('save'),
        event('select',key='b'),event('loaded',saved=True),
        difference({'keys':['a','b'],'opened':1,'slots.a.page.prepared':2},{'keys':['b'],'opened':2,'created':2}),
        event('close'),difference({'keys':['a','b']},{'keys':['b']})],
        repair='Retire hidden non-retained page before deferred deletion; ignore its late loaded.')
    add('repair-loaded-callback-selects-ready', [event('warm',key='b'),checkpoint(),event('open',key='a'),
        difference({'opened':1,'selected':'b'},{'opened':2,'selected':'b'})],
        repair='A loaded callback can select another prepared page without stale delivery state.',
        callbacks=[dict(phase='loaded',token='a',events=[event('selectExisting',key='b')])])
    add('repair-prepare-callback-selects-ready', [event('warm',key='b'),checkpoint(),event('open',key='a'),
        difference({'opened':0,'selected':'b','opening':True},{'opened':1,'selected':'b','opening':False})],
        repair='Preparation callback switches selection after the first pending dispatch.',asynchronous=True,
        callbacks=[dict(phase='prepare',token='a',events=[event('select',key='b')])])
    add('repair-duplicate-onloaded', [event('open',key='a'),checkpoint(opened=1),event('loaded',key='a'),
        difference({'slots.a.page.prepared':2,'slots.a.page.deactivated':2},{'slots.a.page.prepared':1,'slots.a.page.deactivated':1,'opened':1})],
        repair='Duplicate loaded for the same instance cannot repeat preparation/deactivation.')
    add('repair-selected-loader-destruction', [event('open',key='a'),checkpoint(opened=1),event('destroySlot',key='a'),
        difference({'active':True},{'active':False,'opening':False,'keys':[],'failures':1})],
        repair='Externally destroyed selected Loader is removed and failure releases the menu.')
    add('destroy-pool-in-prepare-callback', [event('open',key='a'),checkpoint(live=False,opened=0)],asynchronous=True,
        callbacks=[dict(phase='prepare',token='a',events=[event('set',property='active',value=False),event('destroy')])])
    return result
