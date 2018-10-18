using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class notificationDataManager
    {
        //notificationViewModel
//publicintid{get;set;}
//publicstringmsg{get;set;}
//publicDateTime?add_ts{get;set;}
//publicint?send{get;set;}
//publiclong?uid{get;set;}
//publicstringtype{get;set;}

        public static void Add(notificationViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new notification
            {
                id      = model.id     ,    
                msg     = model.msg    ,
                add_ts  = model.add_ts ,
                send    = model.send   ,
                uid     = model.uid    ,
                type    = model.type   ,
            };

            db.notifications.Add(dbmodel);
        }

        public static void Modify(notificationViewModel model, RAD_PAYEntities db)
        {
            var result = db.notifications.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id     ;    
                    dbmodel.msg = model.msg    ;
                    dbmodel.add_ts = model.add_ts ;
                    dbmodel.send = model.send   ;
                    dbmodel.uid = model.uid    ;
                    dbmodel.type = model.type   ;
                }
            }
        }

        public static void Delete(notificationViewModel model, RAD_PAYEntities db)
        {
            var result = db.notifications.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.notifications.Remove(dbmodel);
                }
            }
        }

        public static List<notificationViewModel> Get(notificationViewModel model, RAD_PAYEntities db)
        {
            List<notificationViewModel> list = null;

            var query = from resmodel in db.notifications
                        select new notificationViewModel
                        {
                            id = resmodel.id,
                            msg = resmodel.msg,
                            add_ts = resmodel.add_ts,
                            send = resmodel.send,
                            uid = resmodel.uid,
                            type = resmodel.type,
                        };

            list = query.ToList();

            return list;
        }
    }
}