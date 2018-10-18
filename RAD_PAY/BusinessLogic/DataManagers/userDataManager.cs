using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class userDataManager
    {
        //userViewModel
                                                                                        //public           long           id                    {           get;           set;           }
                                                                                        //public           string           phone               {           get;           set;           }
                                                                                        //public           string           password           {           get;           set;           }
                                                                                        //public           string           name                {           get;           set;           }
                                                                                        //public           DateTime           registration           {           get;           set;           }
                                                                                        //public           int           sex                    {           get;           set;           }
                                                                                        //public           string           notify_token           {           get;           set;           }
                                                                                        //public           string           qrstring            {           get;           set;           }
                                                                                        //public           int?           tr_limit              {           get;           set;           }
                                                                                        //public           string           avatar              {           get;           set;           }
                                                                                        //public           string           email               {           get;           set;           }
                                                                                        //public           string           token               {           get;           set;           }
                                                                                        //public           int?           lang                  {           get;           set;           }
                                                                                        //public           int           blocked                {           get;           set;           }
                                                                                        //public           DateTime?           purch_ts           {           get;           set;           }
                                                                                        //public           bool           sms_allow             {           get;           set;           }

        public static void Add(userViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new user
            {
                id              = model.id          ,
                phone           = model.phone       ,
                password        = model.password    ,
                name            = model.name        ,
                registration    = model.registration,
                sex             = model.sex         ,
                notify_token    = model.notify_token,
                qrstring        = model.qrstring    ,
                tr_limit        = model.tr_limit    ,
                avatar          = model.avatar      ,
                email           = model.email       ,
                token           = model.token       ,
                lang            = model.lang        ,
                blocked         = model.blocked     ,
                purch_ts        = model.purch_ts    ,
                sms_allow       = model.sms_allow   ,
            };

            db.users.Add(dbmodel);
        }

        public static void Modify(userViewModel model, RAD_PAYEntities db)
        {
            var result = db.users.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id          ;
                    dbmodel.phone = model.phone       ;
                    dbmodel.password = model.password    ;
                    dbmodel.name = model.name        ;
                    dbmodel.registration = model.registration;
                    dbmodel.sex = model.sex         ;
                    dbmodel.notify_token = model.notify_token;
                    dbmodel.qrstring = model.qrstring    ;
                    dbmodel.tr_limit = model.tr_limit    ;
                    dbmodel.avatar = model.avatar      ;
                    dbmodel.email = model.email       ;
                    dbmodel.token = model.token       ;
                    dbmodel.lang = model.lang        ;
                    dbmodel.blocked = model.blocked     ;
                    dbmodel.purch_ts = model.purch_ts    ;
                    dbmodel.sms_allow = model.sms_allow   ;
                }
            }
        }

        public static void Delete(userViewModel model, RAD_PAYEntities db)
        {
            var result = db.users.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.users.Remove(dbmodel);
                }
            }
        }

        public static List<userViewModel> Get(userViewModel model, RAD_PAYEntities db)
        {
            List<userViewModel> list = null;

            var query = from resmodel in db.users
                        select new userViewModel
                        {
                            id = resmodel.id,
                            phone = resmodel.phone,
                            password = resmodel.password,
                            name = resmodel.name,
                            registration = resmodel.registration,
                            sex = resmodel.sex,
                            notify_token = resmodel.notify_token,
                            qrstring = resmodel.qrstring,
                            tr_limit = resmodel.tr_limit,
                            avatar = resmodel.avatar,
                            email = resmodel.email,
                            token = resmodel.token,
                            lang = resmodel.lang,
                            blocked = resmodel.blocked,
                            purch_ts = resmodel.purch_ts,
                            sms_allow = resmodel.sms_allow,
                        };

            list = query.ToList();

            return list;
        }
    }
}