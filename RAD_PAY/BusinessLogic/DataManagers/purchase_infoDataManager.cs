using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class purchase_infoDataManager
    {
        //purchase_infoViewModel
//publiclongrad_tr_id{get;set;}
//publiclongtrn_id{get;set;}
//publicstringjson_text{get;set;}
//publiclong?request_type{get;set;}
//publicstringinput_of_request{get;set;}

        public static void Add(purchase_infoViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new purchase_info
            {
                rad_tr_id           = model.rad_tr_id       ,     
                trn_id              = model.trn_id          ,
                json_text           = model.json_text       ,
                request_type        = model.request_type    ,
                input_of_request    = model.input_of_request,
            };

            db.purchase_info.Add(dbmodel);
        }

        public static void Modify(purchase_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.purchase_info.Where(z => z.rad_tr_id == model.rad_tr_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.rad_tr_id = model.rad_tr_id       ;     
                    dbmodel.trn_id = model.trn_id          ;
                    dbmodel.json_text = model.json_text       ;
                    dbmodel.request_type = model.request_type    ;
                    dbmodel.input_of_request = model.input_of_request;
                }
            }
        }

        public static void Delete(purchase_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.purchase_info.Where(z => z.rad_tr_id == model.rad_tr_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.purchase_info.Remove(dbmodel);
                }
            }
        }

        public static List<purchase_infoViewModel> Get(purchase_infoViewModel model, RAD_PAYEntities db)
        {
            List<purchase_infoViewModel> list = null;

            var query = from resmodel in db.purchase_info
                        select new purchase_infoViewModel
                        {
                            rad_tr_id = resmodel.rad_tr_id,
                            trn_id = resmodel.trn_id,
                            json_text = resmodel.json_text,
                            request_type = resmodel.request_type,
                            input_of_request = resmodel.input_of_request,
                        };

            list = query.ToList();

            return list;
        }
    }
}