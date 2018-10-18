using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class users_full_registersDataManager
    {
        //users_full_registersViewModel
//publiclongid{get;set;}
//publiclong?uid{get;set;}
//publicstringfio{get;set;}
//publicstringpassport_number{get;set;}
//publicDateTime?passport_start_date{get;set;}
//publicDateTime?passport_end_date{get;set;}
//publicstringpassport_image_path{get;set;}
//publicDateTime?date_of_birth{get;set;}
//publicstringnationality{get;set;}
//publicstringcitizenship{get;set;}
//publicint?status{get;set;}
//publicstringpassport_serial{get;set;}
//publicint?level{get;set;}

        public static void Add(users_full_registersViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new users_full_registers
            {
                id                  = model.id                  ,           
                uid                 = model.uid                 ,
                fio                 = model.fio                 ,
                passport_number     = model.passport_number     ,
                passport_start_date = model.passport_start_date ,
                passport_end_date   = model.passport_end_date   ,
                passport_image_path = model.passport_image_path ,
                date_of_birth       = model.date_of_birth       ,
                nationality         = model.nationality         ,
                citizenship         = model.citizenship         ,
                status              = model.status              ,
                passport_serial     = model.passport_serial     ,
                level               = model.level               ,
            };

            db.users_full_registers.Add(dbmodel);
        }

        public static void Modify(users_full_registersViewModel model, RAD_PAYEntities db)
        {
            var result = db.users_full_registers.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                  ;           
                    dbmodel.uid = model.uid                 ;
                    dbmodel.fio = model.fio                 ;
                    dbmodel.passport_number = model.passport_number     ;
                    dbmodel.passport_start_date = model.passport_start_date ;
                    dbmodel.passport_end_date = model.passport_end_date   ;
                    dbmodel.passport_image_path = model.passport_image_path ;
                    dbmodel.date_of_birth = model.date_of_birth       ;
                    dbmodel.nationality = model.nationality         ;
                    dbmodel.citizenship = model.citizenship         ;
                    dbmodel.status = model.status              ;
                    dbmodel.passport_serial = model.passport_serial     ;
                    dbmodel.level = model.level               ;
                }
            }
        }

        public static void Delete(users_full_registersViewModel model, RAD_PAYEntities db)
        {
            var result = db.users_full_registers.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.users_full_registers.Remove(dbmodel);
                }
            }
        }

        public static List<users_full_registersViewModel> Get(users_full_registersViewModel model, RAD_PAYEntities db)
        {
            List<users_full_registersViewModel> list = null;

            var query = from resmodel in db.users_full_registers
                        select new users_full_registersViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            fio = resmodel.fio,
                            passport_number = resmodel.passport_number,
                            passport_start_date = resmodel.passport_start_date,
                            passport_end_date = resmodel.passport_end_date,
                            passport_image_path = resmodel.passport_image_path,
                            date_of_birth = resmodel.date_of_birth,
                            nationality = resmodel.nationality,
                            citizenship = resmodel.citizenship,
                            status = resmodel.status,
                            passport_serial = resmodel.passport_serial,
                            level = resmodel.level,
                        };

            list = query.ToList();

            return list;
        }
    }
}