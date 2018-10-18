using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_groupDataManager
    {
        //merchant_groupViewModel
//publicintid{get;set;}
//publicstringname{get;set;}
//publicint?position{get;set;}
//publicstringname_uzb{get;set;}
//publicstringicon_path{get;set;}
//publiclong?icon_id{get;set;}

        public static void Add(merchant_groupViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant_group
            {
                    id          = model.id          ,  
                    name        = model.name        ,
                    position    = model.position    ,
                    name_uzb    = model.name_uzb    ,
                    icon_path   = model.icon_path   ,
                    icon_id     = model.icon_id     ,
            };

            db.merchant_group.Add(dbmodel);
        }

        public static void Modify(merchant_groupViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_group.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id          ;  
                    dbmodel.name = model.name        ;
                    dbmodel.position = model.position    ;
                    dbmodel.name_uzb = model.name_uzb    ;
                    dbmodel.icon_path = model.icon_path   ;
                    dbmodel.icon_id = model.icon_id     ;
                }
            }
        }

        public static void Delete(merchant_groupViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_group.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchant_group.Remove(dbmodel);
                }
            }
        }

        public static List<merchant_groupViewModel> Get(merchant_groupViewModel model, RAD_PAYEntities db)
        {
            List<merchant_groupViewModel> list = null;

            var query = from resmodel in db.merchant_group
                        select new merchant_groupViewModel
                        {
                            id = resmodel.id,
                            name = resmodel.name,
                            position = resmodel.position,
                            name_uzb = resmodel.name_uzb,
                            icon_path = resmodel.icon_path,
                            icon_id = resmodel.icon_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}