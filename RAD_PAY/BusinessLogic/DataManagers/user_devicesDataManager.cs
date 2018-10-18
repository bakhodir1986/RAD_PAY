using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class user_devicesDataManager
    {
        //user_devicesViewModel
                                                                                                //public            long            dev_id            {            get;            set;            }
                                                                                                //public            long?            uid            {            get;            set;            }
                                                                                                //public            string            dev_token            {            get;            set;            }
                                                                                                //public            string            password            {            get;            set;            }
                                                                                                //public            DateTime?            add_ts            {            get;            set;            }
                                                                                                //public            string            notify_token            {            get;            set;            }
                                                                                                //public            string            os            {            get;            set;            }
                                                                                                //public            DateTime?            login_ts            {            get;            set;            }
                                                                                                //public            string            dev_imei            {            get;            set;            }

        public static void Add(user_devicesViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new user_devices
            {
                    dev_id         = model.dev_id       ,
                    uid            = model.uid          ,
                    dev_token      = model.dev_token    ,
                    password       = model.password     ,
                    add_ts         = model.add_ts       ,
                    notify_token   = model.notify_token ,
                    os             = model.os           ,
                    login_ts       = model.login_ts     ,
                    dev_imei       = model.dev_imei     ,
            };

            db.user_devices.Add(dbmodel);
        }

        public static void Modify(user_devicesViewModel model, RAD_PAYEntities db)
        {
            var result = db.user_devices.Where(z => z.dev_id == model.dev_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.dev_id = model.dev_id       ;
                    dbmodel.uid = model.uid          ;
                    dbmodel.dev_token = model.dev_token    ;
                    dbmodel.password = model.password     ;
                    dbmodel.add_ts = model.add_ts       ;
                    dbmodel.notify_token = model.notify_token ;
                    dbmodel.os = model.os           ;
                    dbmodel.login_ts = model.login_ts     ;
                    dbmodel.dev_imei = model.dev_imei     ;
                }
            }
        }

        public static void Delete(user_devicesViewModel model, RAD_PAYEntities db)
        {
            var result = db.user_devices.Where(z => z.dev_id == model.dev_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.user_devices.Remove(dbmodel);
                }
            }
        }

        public static List<user_devicesViewModel> Get(user_devicesViewModel model, RAD_PAYEntities db)
        {
            List<user_devicesViewModel> list = null;

            var query = from resmodel in db.user_devices
                        select new user_devicesViewModel
                        {
                            dev_id = resmodel.dev_id,
                            uid = resmodel.uid,
                            dev_token = resmodel.dev_token,
                            password = resmodel.password,
                            add_ts = resmodel.add_ts,
                            notify_token = resmodel.notify_token,
                            os = resmodel.os,
                            login_ts = resmodel.login_ts,
                            dev_imei = resmodel.dev_imei,
                        };

            list = query.ToList();

            return list;
        }
    }
}