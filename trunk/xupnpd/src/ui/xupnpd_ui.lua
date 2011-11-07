ui_args=nil
ui_data=nil

function ui_main()
    http.sendtfile(cfg.ui_path..'ui_main.html',http_vars)
end

function ui_error()
    http.send('<h2>Error occurred</h3>')
    http.send('<br><a class="btn info" href="/ui">Back</a>')
end

function ui_downloads()
    http.send('<h3>Downloads</h3>')
    http.send('<br><table>')
    for i,j in ipairs(playlist_data.elements) do
        http.send(string.format('<tr><td><a href="/ui/%s.m3u">%s</a></td></tr>',j.name,j.name))
    end
    http.send('</table>')
    http.send('<br><a class="btn info" href="/ui">Back</a>')
end

function ui_download(name)
    name=util.urldecode(string.match(name,'(.+)%.m3u$'))

    local pls=nil

    for i,j in ipairs(playlist_data.elements) do
        if j.name==name then pls=j break end
    end

    if not pls then
        http.send(
            string.format(
                'HTTP/1.1 404 Not found\r\nPragma: no-cache\r\nCache-control: no-cache\r\nDate: %s\r\nServer: %s\r\n'..
                'Connection: close\r\n\r\n',os.date('!%a, %d %b %Y %H:%M:%S GMT'),ssdp_server)
        )
        return
    end

    http.send(
        string.format(
            'HTTP/1.1 200 Ok\r\nPragma: no-cache\r\nCache-control: no-cache\r\nDate: %s\r\nServer: %s\r\nAccept-Ranges: none\r\n'..
            'Connection: close\r\nContent-Type: audio/x-mpegurl\r\n\r\n',os.date('!%a, %d %b %Y %H:%M:%S GMT'),ssdp_server)
    )

    http.send('#EXTM3U\n')
    for i,j in ipairs(pls.elements) do
        local url=j.url or ''
        if j.path then
            url=www_location..'/stream?s='..util.urlencode(j.objid)
        else
            if cfg.proxy>0 then
                if cfg.proxy>1 or pls.mime[1]==2 then
                    url=www_location..'/proxy?s='..util.urlencode(j.objid)
                end
            end
        end
        http.send('#EXTINF:0,'..j.name..'\n'..url..'\n')
    end
end

function ui_playlists()
    http.send('<h3>Playlists</h3>')
    http.send('<br><table>')

    local d=util.dir(cfg.playlists_path)
    if d then
        table.sort(d)
        for i,j in ipairs(d) do
            if string.find(j,'.+%.m3u$') then
                local fname=util.urlencode(j)
                http.send(string.format('<tr><td><a href=\'/ui/show?fname=%s\'>%s</a> [<a href=\'/ui/remove?fname=%s\'>x</a>]</td></tr>\n',fname,j,fname))
            end
        end
    end

    http.send('</table>')

    http.send('<br><h3>Upload *.m3u file</h3>')
    http.send('<form method=post action="/ui/upload" enctype="multipart/form-data">')
    http.send('<input type=file name=m3ufile>')
    http.send('<input class="btn Primary" type=submit value=Send>')
    http.send('</form><hr>')
    http.send('<br><a class="btn Primary" href="/ui/reload">Reload</a> <a class="btn Primary" href="/ui/feeds">Reload feeds</a> <a class="btn info" href="/ui">Back</a>')
end

function ui_show()
    if ui_args.fname and string.find(ui_args.fname,'^[%w_]+%.m3u$') then
        local pls=m3u.parse(cfg.playlists_path..ui_args.fname)

        if pls then
            http.send('<h3>'..pls.name..'</h3>')
            http.send('<br><table>')
            for i,j in ipairs(pls.elements) do
                http.send(string.format('<tr><td><a href="%s">%s</a></td></tr>',j.url,j.name))
            end
            http.send('</table>')
        end
    end

    http.send('<br><a class="btn info" href="/ui/playlists">Back</a>')
end

function ui_remove()
    if ui_args.fname and string.find(ui_args.fname,'^[%w_]+%.m3u$') then
        if os.remove(cfg.playlists_path..ui_args.fname) then
            core.sendevent('reload')
            http.send('<h3>OK</h3>')
        else
            http.send('<h3>Fail</h3>')
        end
    end

    http.send('<br><a class="btn info" href="/ui/playlists">Back</a>')
end

function ui_reload()
    core.sendevent('reload')
    http.send('<h3>OK</h3>')
    http.send('<br><a class="btn info" href="/ui/playlists">Back</a>')
end

function ui_reload_feeds()
    update_feeds_async()
    http.send('<h3>OK</h3>')
    http.send('<br><a class="btn info" href="/ui/playlists">Back</a>')
end

function ui_config()
    http_vars.youtube_fmt=cfg.youtube_fmt
    http_vars.youtube_region=cfg.youtube_region
    http_vars.ivi_fmt=cfg.ivi_fmt
    http.sendtfile(cfg.ui_path..'ui_config.html',http_vars)
end

function ui_apply()
    local f=io.open(cfg.config_path..'common.lua','w')
    if f then
        f:write('cfg.youtube_fmt="',ui_args.youtube_q or '18','"\ncfg.ivi_fmt="',ui_args.ivi_q or 'MP4-lo','"\ncfg.youtube_region="',ui_args.youtube_r or '*','"\n')
        f:close()
        core.sendevent('config')
    end

    http.send('<h3>OK</h3>')
    http.send('<br><a class="btn info" href="/ui/config">Back</a>')
end

function ui_status()
    http.send('<h3>Status</h3>')
    http.send('<br><table>')

    for i,j in pairs(childs) do
        if j.status then
            http.send(string.format('<tr><td>%s [<a href="/ui/kill?pid=%s">x</a>]</td></tr>',j.status,i))
        end
    end

    http.send('</table>')

    http.send('<br><a class="btn primary" href="/ui/status">Refresh</a> <a class="btn info" href="/ui">Back</a>')
end

function ui_kill()
    if ui_args.pid and childs[tonumber(ui_args.pid)] then
        util.kill(ui_args.pid)
        http.send('<h3>OK</h3>')
    else
        http.send('<h3>Fail</h3>')
    end
    http.send('<br><a class="btn info" href="/ui/status">Back</a>')
end

function ui_upload()
    local tt=util.multipart_split(ui_data)
    ui_data=nil

    if tt and tt[1] then
        local n,m=string.find(tt[1],'\r?\n\r?\n')

        if n then
            local fname=string.match(string.sub(tt[1],1,n-1),'filename=\"(.+)\"')

            if fname and string.find(fname,'.+%.m3u$') then
                local tfname=cfg.tmp_path..fname

                local fd=io.open(tfname,'w+')
                if fd then
                    fd:write(string.sub(tt[1],m+1))
                    fd:close()
                end

                local pls=m3u.parse(tfname)

                if pls then
                    if os.execute(string.format('mv %s %s',tfname,cfg.playlists_path..fname))~=0 then
                        os.remove(tfname)
                        http.send('<h3>Fail</h3>')
                    else
                        core.sendevent('reload')
                        http.send('<h3>OK</h3>')
                    end
                else
                    os.remove(tfname)
                    http.send('<h3>Fail</h3>')
                end
            else
                http.send('<h3>Fail</h3>')
            end
        end
    end

    http.send('<br><a class="btn info" href="/ui/playlists">Back</a>')
end

ui_actions=
{
    ['main']            = { 'xupnpd', ui_main },
    ['error']           = { 'xupnpd - error', ui_error },
    ['downloads']       = { 'xupnpd - downloads', ui_downloads },
    ['playlists']       = { 'xupnpd - playlists', ui_playlists },
    ['show']            = { 'xupnpd - show', ui_show },
    ['remove']          = { 'xupnpd - remove', ui_remove },
    ['reload']          = { 'xupnpd - reload', ui_reload },
    ['feeds']           = { 'xupnpd - reload feeds', ui_reload_feeds },
    ['config']          = { 'xupnpd - config', ui_config },
    ['status']          = { 'xupnpd - status', ui_status },
    ['kill']            = { 'xupnpd - kill', ui_kill },
    ['upload']          = { 'xupnpd - upload', ui_upload },
    ['apply']           = { 'xupnpd - apply', ui_apply }
}

function ui_handler(args,data,ip,url)
    local action=string.match(url,'^/ui/(.+)$')

    if action=='style' then
        http_send_headers(200,'css')
        http.sendfile(cfg.ui_path..'bootstrap.min.css')
        return
    end

    if action and string.find(action,'.+%.m3u$') then
        ui_download(action)
        return
    end

    if not action then action='main' end

    http_send_headers(200,'html')

    local act=ui_actions[action]

    if not act then act=ui_actions['error'] end

    http_vars.title=act[1]
    http_vars.content=act[2]

    ui_args=args
    ui_data=data

    http.sendtfile(cfg.ui_path..'ui_template.html',http_vars)
end
