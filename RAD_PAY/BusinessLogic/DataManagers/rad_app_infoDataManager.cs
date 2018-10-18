using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class rad_app_infoDataManager
    {
        //rad_app_infoViewModel
//publicintid{get;set;}
//publicstringversion{get;set;}
//publicstringos{get;set;}
//publicDateTime?release_date{get;set;}
//publicDateTime?expiry_date{get;set;}
//publicstringmin_version{get;set;}
//publicstringa3{get;set;}

        public static void Add(rad_app_infoViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new rad_app_info
            {
                id              = model.id              ,
                version         = model.version         ,
                os              = model.os              ,
                release_date    = model.release_date    ,
                expiry_date     = model.expiry_date     ,
                min_version     = model.min_version     ,
                a3              = model.a3              ,
            };

            db.rad_app_info.Add(dbmodel);
        }

        public static void Modify(rad_app_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.rad_app_info.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id              ;
                    dbmodel.version = model.version         ;
                    dbmodel.os = model.os              ;
                    dbmodel.release_date = model.release_date    ;
                    dbmodel.expiry_date = model.expiry_date     ;
                    dbmodel.min_version = model.min_version     ;
                    dbmodel.a3 = model.a3              ;
                }
            }
        }

        public static void Delete(rad_app_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.rad_app_info.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.rad_app_info.Remove(dbmodel);
                }
            }
        }

        public static List<rad_app_infoViewModel> Get(rad_app_infoViewModel model, RAD_PAYEntities db)
        {
            List<rad_app_infoViewModel> list = null;

            var query = from resmodel in db.rad_app_info
                        select new rad_app_infoViewModel
                        {
                            id = resmodel.id,
                            version = resmodel.version,
                            os = resmodel.os,
                            release_date = resmodel.release_date,
                            expiry_date = resmodel.expiry_date,
                            min_version = resmodel.min_version,
                            a3 = resmodel.a3,
                        };

            list = query.ToList();

            return list;
        }
    }
}